"""fprime-gds framing plugin: Space Packets segmented into Type-BD TC frames with Segment Headers.

Selection plugin (inert unless `--framing-selection tc-segment`). Load it with

    PYTHONPATH=<repo>/Svc/Ccsds/TcMapReassembler/test/int
    FPRIME_GDS_EXTRA_PLUGINS=tc_segment_plugin.framing:TcSegmentFraming
    fprime-gds ... --framing-selection tc-segment [--tc-segment-sdls-key-file <32-octet key>]

Uplink: F Prime data -> Space Packet (stock `SpacePacketFramerDeframer`) -> one or more TC frames, each
`PrimHdr | SH | [SPI | IV | ciphertext | MAC] | FECF`, portions of at most 1016 (clear) or 986 (SDLS)
octets. With a key file every segment is AES-256-GCM protected under SPI 1 (default) with a 20-octet
AAD that authenticates the VCID, the Segment Header and the SPI; the IV is a 96-bit frame counter whose
low octet is the frame sequence number. Downlink: stock TM deframing; when SDLS is on, the 2-octet
Security Association index that `Svc.Ccsds.CcsdsSdlsFramer` prepends is removed before Space Packet
decoding (`Svc.Ccsds.ClearTextEncryptor` leaves the frame data in clear).

Fault injection for the integration tests is steered through `control.Knobs` (see control.py).
"""

import os
import struct
from pathlib import Path
from typing import List, Optional, Tuple

from fprime_gds.common.communication.ccsds.space_data_link import (
    SpaceDataLinkFramerDeframer,
)
from fprime_gds.common.communication.ccsds.space_packet import SpacePacketFramerDeframer
from fprime_gds.common.communication.framing import FramerDeframer
from fprime_gds.plugin.definitions import gds_plugin

from . import tc_frames as tf
from .control import (
    ENV_SDLS_KEY_FILE,
    ENV_SDLS_SPI,
    Knobs,
    control_file_path,
    read_knobs,
)

SDLS_KEY_SIZE = 32
SA_INDEX_SIZE = 2
SPACE_PACKET_HEADER_SIZE = 6
DEFAULT_SPI = 1
DEFAULT_MAP_ID = 0
DEFAULT_VCID = 1


def _int_arg(text: str) -> int:
    return int(text, 0)


@gds_plugin(FramerDeframer)
class TcSegmentFraming(FramerDeframer):
    """Segment-Header TC framing (CCSDS 232.0-B-4) with optional SDLS (355.0-B-2) for GDS uplink."""

    def __init__(
        self,
        tc_segment_scid: Optional[int] = None,
        tc_segment_vcid: int = DEFAULT_VCID,
        tc_segment_map_id: int = DEFAULT_MAP_ID,
        tc_segment_sdls_key_file: Optional[str] = None,
        tc_segment_sdls_spi: int = DEFAULT_SPI,
        tc_segment_control_file: Optional[str] = None,
    ):
        # Stock layers: Space Packet coding, and the TM (downlink) side of the Space Data Link protocol.
        # SCID/frame size default to the dictionary constants exactly as the stock chain does.
        self.space_packet = SpacePacketFramerDeframer()
        self.link = SpaceDataLinkFramerDeframer(tc_segment_scid, tc_segment_vcid, None)
        self.scid = self.link.scid
        self.vcid = tc_segment_vcid
        self.map_id = tc_segment_map_id
        self.spi = tc_segment_sdls_spi
        self.key = self._load_key(tc_segment_sdls_key_file)
        self.control_file = control_file_path(tc_segment_control_file)
        self.fsn = 0
        self.iv_counter = 0

    @property
    def sdls(self) -> bool:
        return self.key is not None

    @property
    def max_portion(self) -> int:
        return tf.MAX_PORTION_SDLS if self.sdls else tf.MAX_PORTION_CLEAR

    # ------------------------------------------------------------------
    # Uplink
    # ------------------------------------------------------------------

    def frame(self, data: bytes) -> bytes:
        """Frame one F Prime uplink data unit into a sequence of TC frames (returned concatenated)."""
        knobs = read_knobs(self.control_file)
        if knobs.payload is not None:
            data = self._synthetic_payload(knobs.payload["apid"], knobs.payload["size"])
        packet = self.space_packet.frame(data)
        return b"".join(self.frames_for_packet(packet, knobs))

    def frames_for_packet(self, packet: bytes, knobs: Knobs) -> List[bytes]:
        """Segment `packet`, apply the fault-injection knobs, and wrap every emitted segment in a frame."""
        map_id = self.map_id if knobs.map_id is None else knobs.map_id
        spi = self.spi if knobs.spi is None else knobs.spi
        portion = (
            self.max_portion
            if knobs.segment_size is None
            else min(knobs.segment_size, self.max_portion)
        )
        segments = tf.segment_packet(packet, map_id, portion)

        frames: List[bytes] = []
        if knobs.control_frame:
            frames.append(self._emit(tf.control_frame(self.scid, self.vcid, self.fsn)))
        if knobs.empty_segment:
            frames.append(
                self._frame(tf.Segment(tf.FLAG_UNSEGMENTED, map_id, b""), spi)
            )

        for index in self._emission_order(len(segments), knobs):
            corrupt = index in knobs.corrupt_mac
            tamper = (
                knobs.tamper_sh["value"]
                if knobs.tamper_sh and knobs.tamper_sh["index"] == index
                else None
            )
            frames.append(
                self._frame(
                    segments[index],
                    spi,
                    sh_override=tamper,
                    corrupt_mac=corrupt,
                    strip_sh=knobs.strip_sh,
                )
            )
            if index in knobs.retransmit:
                frames.append(
                    self._frame(segments[index], spi, strip_sh=knobs.strip_sh)
                )
        return frames

    @staticmethod
    def _emission_order(count: int, knobs: Knobs) -> List[int]:
        order = list(range(count))
        if knobs.swap:
            first, second = knobs.swap
            order[first], order[second] = order[second], order[first]
        if knobs.duplicate_first:
            order.insert(0, 0)
        if knobs.omit_first:
            order = [index for index in order if index != 0]
        if knobs.omit_last:
            order = [index for index in order if index != count - 1]
        return [index for index in order if index not in knobs.drop]

    def _frame(self, segment: tf.Segment, spi: int, **options) -> bytes:
        frame = tf.frame_segment(
            self.scid,
            self.vcid,
            self.fsn,
            segment,
            sdls_key=self.key,
            spi=spi,
            iv_counter=self.iv_counter,
            **options,
        )
        return self._emit(frame)

    def _emit(self, frame: bytes) -> bytes:
        """Account for one transmitted frame: FSN modulo 256, IV counter never reused."""
        self.fsn = (self.fsn + 1) % tf.FSN_MODULUS
        self.iv_counter += 1
        return frame

    @staticmethod
    def _synthetic_payload(apid: int, size: int) -> bytes:
        """Data whose Space Packet totals `size` octets: descriptor (the APID) followed by a counting fill."""
        data_size = size - SPACE_PACKET_HEADER_SIZE
        if data_size < struct.calcsize(">H"):
            raise ValueError(
                f"synthetic packet of {size} octets cannot hold an APID descriptor"
            )
        fill = bytes(
            (index & 0xFF) for index in range(data_size - struct.calcsize(">H"))
        )
        return struct.pack(">H", apid) + fill

    # ------------------------------------------------------------------
    # Downlink
    # ------------------------------------------------------------------

    def deframe(self, data, no_copy=False):
        raise AssertionError(
            "deframe_all is the only entry point of this composite framer"
        )

    def deframe_all(self, data, no_copy):
        """TM frames -> [SA index removal] -> Space Packets -> F Prime packets."""
        frames, remaining, discarded = self.link.deframe_all(data, no_copy)
        packets: List[bytes] = []
        for frame in frames:
            frame, dropped = self._strip_sa_index(frame)
            discarded += dropped
            new_packets, _, more_discarded = self.space_packet.deframe_all(frame, True)
            packets.extend(new_packets)
            discarded += more_discarded
        return packets, remaining, discarded

    def _strip_sa_index(self, frame: bytes) -> Tuple[bytes, bytes]:
        if not self.sdls:
            return frame, b""
        if len(frame) < SA_INDEX_SIZE:
            return b"", frame
        return frame[SA_INDEX_SIZE:], b""

    # ------------------------------------------------------------------
    # Plugin interface
    # ------------------------------------------------------------------

    @classmethod
    def get_name(cls):
        return "tc-segment"

    @classmethod
    def get_arguments(cls):
        return {
            ("--tc-segment-scid",): {
                "type": _int_arg,
                "help": "Spacecraft ID (overrides the dictionary ComCfg.SpacecraftId)",
                "required": False,
            },
            ("--tc-segment-vcid",): {
                "type": _int_arg,
                "default": DEFAULT_VCID,
                "help": "Virtual channel ID for TC frames and TM deframing",
                "required": False,
            },
            ("--tc-segment-map-id",): {
                "type": _int_arg,
                "default": DEFAULT_MAP_ID,
                "help": "MAP ID carried in every Segment Header",
                "required": False,
            },
            ("--tc-segment-sdls-key-file",): {
                "type": str,
                "default": os.environ.get(ENV_SDLS_KEY_FILE),
                "help": f"32-octet AES-256 key file; enables SDLS AES-GCM (default: ${ENV_SDLS_KEY_FILE})",
                "required": False,
            },
            ("--tc-segment-sdls-spi",): {
                "type": _int_arg,
                "default": _int_arg(os.environ.get(ENV_SDLS_SPI, str(DEFAULT_SPI))),
                "help": f"SDLS Security Parameter Index (default: ${ENV_SDLS_SPI} or {DEFAULT_SPI})",
                "required": False,
            },
            ("--tc-segment-control-file",): {
                "type": str,
                "help": "Fault-injection control file read per uplinked packet (see tc_segment_plugin/control.py)",
                "required": False,
            },
        }

    @classmethod
    def check_arguments(
        cls,
        tc_segment_scid=None,
        tc_segment_vcid=DEFAULT_VCID,
        tc_segment_map_id=DEFAULT_MAP_ID,
        tc_segment_sdls_key_file=None,
        tc_segment_sdls_spi=DEFAULT_SPI,
        tc_segment_control_file=None,
    ):
        SpaceDataLinkFramerDeframer.check_arguments(
            tc_segment_scid, tc_segment_vcid, None
        )
        if not 0 <= tc_segment_map_id <= tf.MAP_ID_MAX:
            raise TypeError(
                f"MAP ID {tc_segment_map_id} out of range 0..{tf.MAP_ID_MAX}"
            )
        if not 0 <= tc_segment_sdls_spi <= 0xFFFF:
            raise TypeError(f"SPI {tc_segment_sdls_spi} out of range 0..65535")
        if tc_segment_sdls_key_file is not None:
            cls._load_key(tc_segment_sdls_key_file)

    @staticmethod
    def _load_key(path: Optional[str]) -> Optional[bytes]:
        if path is None or path == "":
            return None
        key = Path(path).read_bytes()
        if len(key) != SDLS_KEY_SIZE:
            raise TypeError(
                f"SDLS key file {path} holds {len(key)} octets, expected {SDLS_KEY_SIZE}"
            )
        return key
