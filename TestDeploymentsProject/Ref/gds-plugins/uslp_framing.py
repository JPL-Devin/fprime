"""GDS Framer/Deframer plugin implementing the CCSDS Unified Space Data Link Protocol (USLP).

Frames uplink data in variable-length USLP transfer frames (TFDF construction
rule 0b111, no segmentation) and deframes fixed-length downlink USLP frames
(TFDF construction rule 0b000, packets spanning frames) as produced by the
Svc.Ccsds.UslpFramer flight component. See CCSDS 732.1-B-3.

Load with:
    FPRIME_GDS_EXTRA_PLUGINS="uslp_framing:UslpFramerDeframer;uslp_framing:SpacePacketUslpFramerDeframer"
"""

import copy
import struct
import sys

import crcmod

from fprime_gds.common.communication.ccsds.chain import ChainedFramerDeframer
from fprime_gds.common.communication.ccsds.space_packet import SpacePacketFramerDeframer
from fprime_gds.common.communication.framing import FramerDeframer
from fprime_gds.common.utils.config_manager import ConfigBadTypeException, ConfigManager
from fprime_gds.plugin.definitions import gds_plugin_implementation


class UslpFramerDeframer(FramerDeframer):
    """CCSDS USLP Framer/Deframer.

    Framing (uplink): variable-length non-truncated frames, TFDF rule 0b111
    (no segmentation), no VCF count, mandatory CRC-16 FECF.
    Deframing (downlink): fixed-length non-truncated frames, 4-octet VCF count,
    TFDF rule 0b000 with a 2-octet First Header Pointer, Encapsulation Idle
    Packet fill, mandatory CRC-16 FECF.
    """

    HEADER_SIZE = 7
    TRAILER_SIZE = 2
    TFVN = 0b1100
    # Downlink (flight-to-ground) TFDF: 1-octet TFDF header + 2-octet FHP
    DOWNLINK_VCF_COUNT_LENGTH = 4
    DOWNLINK_TFDF_HEADER_SIZE = 3
    # Encapsulation Packet Protocol first-octet version bits (0b111)
    EPP_VERSION = 0b111

    # As per CCSDS standard, CRC-16 CCITT with init value all 1s and final XOR of 0x0000
    CCITT_CRC_FUNCTION = crcmod.mkCrcFun(0x11021, initCrc=0xFFFF, xorOut=0x0000, rev=False)

    # For backwards compatibility if not found in dictionary (loaded by ConfigManager)
    FALLBACK_SCID = 0x44
    FALLBACK_FRAME_SIZE = 1024

    def __init__(self, scid=None, vcid=None, map_id=0, frame_size=None):
        dict_scid = None
        dict_frame_size = None
        try:
            dict_scid = ConfigManager().get_constant("ComCfg.SpacecraftId")
        except ConfigBadTypeException:
            pass  # Config value not found, move on
        try:
            dict_frame_size = ConfigManager().get_constant("ComCfg.UslpFrameFixedSize")
        except ConfigBadTypeException:
            pass  # Config value not found, move on
        if scid is not None and dict_scid is not None and scid != dict_scid:
            print(
                f"[WARNING] SCID value specified through CLI argument does not match value"
                f" loaded from the dictionary. CLI={scid}, Dictionary={dict_scid}",
                file=sys.stderr,
            )
        if frame_size is not None and dict_frame_size is not None and frame_size != dict_frame_size:
            print(
                f"[WARNING] USLP frame size value specified through CLI argument does not match value"
                f" loaded from the dictionary. CLI={frame_size}, Dictionary={dict_frame_size}",
                file=sys.stderr,
            )
        # Priority order: command line arg > dictionary value > fallback value
        self.scid = scid or dict_scid or self.FALLBACK_SCID
        self.vcid = 1 if vcid is None else vcid
        self.map_id = map_id
        self.frame_size = frame_size or dict_frame_size or self.FALLBACK_FRAME_SIZE

    def _primary_header(self, source_or_dest, total_length, flags):
        """Build the 7-octet USLP transfer frame primary header"""
        id_word = (
            (self.TFVN << 28)
            | ((self.scid & 0xFFFF) << 12)
            | ((source_or_dest & 0x1) << 11)
            | ((self.vcid & 0x3F) << 5)
            | ((self.map_id & 0xF) << 1)
            | 0  # End of Frame Primary Header flag: 0 = non-truncated frame
        )
        # Frame Length field carries total octets minus 1
        return struct.pack(">IHB", id_word, total_length - 1, flags)

    def frame(self, data):
        """Frame the supplied data in a variable-length USLP transfer frame"""
        total_length = self.HEADER_SIZE + 1 + len(data) + self.TRAILER_SIZE
        assert total_length <= 0x10000, "Data too large for USLP frame"
        # Flags: bypass/expedited (1) | protocol command (0) | spares (00) |
        #        OCF (0) | VCF count length (000 = no VCF count on uplink)
        flags = 0x80
        header = self._primary_header(1, total_length, flags)  # 1 = spacecraft is destination
        # TFDF header: construction rule 0b111 (no segmentation), UPID 0b00000
        tfdf_header = struct.pack(">B", 0b111 << 5)
        frame_no_crc = header + tfdf_header + bytes(data)
        return frame_no_crc + struct.pack(">H", UslpFramerDeframer.CCITT_CRC_FUNCTION(frame_no_crc))

    def _strip_idle_fill(self, zone):
        """Trim the trailing Encapsulation Idle Packet fill from a TFDZ.

        The flight framer places exactly one space packet at the start of the
        TFDZ followed by an optional Encapsulation Idle Packet. Walk the space
        packets and stop at the first Encapsulation Packet version octet.
        """
        offset = 0
        while offset + 6 <= len(zone):
            if (zone[offset] >> 5) == self.EPP_VERSION:
                break
            # Space packet: data length field is total data octets minus 1
            data_length = struct.unpack_from(">H", zone, offset + 4)[0]
            offset += 6 + data_length + 1
        return zone[:offset] if offset <= len(zone) else zone

    def deframe(self, data, no_copy=False):
        """Deframe fixed-length USLP transfer frames"""
        discarded = bytearray()
        if not no_copy:
            data = copy.copy(data)
        data = memoryview(data)
        while len(data) >= self.frame_size:
            id_word = struct.unpack_from(">I", data)[0]
            tfvn = (id_word >> 28) & 0xF
            scid = (id_word >> 12) & 0xFFFF
            vcid = (id_word >> 5) & 0x3F
            if tfvn != self.TFVN or scid != self.scid or vcid != self.vcid:
                # If the header is invalid, rotate away a byte and keep processing
                discarded += data[0:1]
                data = data[1:]
                continue
            crc_offset = self.frame_size - self.TRAILER_SIZE
            transmitted_crc = struct.unpack_from(">H", data, crc_offset)[0]
            if transmitted_crc == UslpFramerDeframer.CCITT_CRC_FUNCTION(bytes(data[:crc_offset])):
                zone_offset = self.HEADER_SIZE + self.DOWNLINK_VCF_COUNT_LENGTH + self.DOWNLINK_TFDF_HEADER_SIZE
                zone = bytes(data[zone_offset:crc_offset])
                deframed = self._strip_idle_fill(zone)
                # Consume the fixed size frame
                data = data[self.frame_size:]
                return deframed, bytes(data), bytes(discarded)
            print("[WARNING] USLP checksum validation failed.", file=sys.stderr)
            # Bad checksum, rotate 1 and keep looking for non-garbage
            discarded += data[0:1]
            data = data[1:]
        return None, bytes(data), bytes(discarded)

    @classmethod
    def get_name(cls):
        """Name of this implementation provided to CLI"""
        return "uslp"

    @classmethod
    def get_arguments(cls):
        """Arguments to request from the CLI"""
        return {
            ("--scid",): {
                "type": lambda input_arg: int(input_arg, 0),
                "help": "Spacecraft ID (if specified, overrides dictionary ComCfg value)",
                "required": False,
            },
            ("--vcid",): {
                "type": lambda input_arg: int(input_arg, 0),
                "help": "Virtual channel ID",
                "default": 1,
                "required": False,
            },
            ("--map-id",): {
                "type": lambda input_arg: int(input_arg, 0),
                "help": "USLP MAP ID",
                "default": 0,
                "required": False,
            },
            ("--frame-size",): {
                "type": lambda input_arg: int(input_arg, 0),
                "help": "Fixed size of downlink USLP frames (if specified, overrides dictionary ComCfg value)",
                "required": False,
            },
        }

    @classmethod
    def check_arguments(cls, scid, vcid, map_id, frame_size):
        """Check arguments from the CLI"""
        if scid is not None and not (0 <= scid <= 0xFFFF):
            raise TypeError(f"Spacecraft ID {scid} out of range [0, {0xFFFF}]")
        if vcid is None or not (0 <= vcid <= 0x3F):
            raise TypeError(f"Virtual Channel ID {vcid} out of range [0, {0x3F}]")
        if map_id is None or not (0 <= map_id <= 0xF):
            raise TypeError(f"MAP ID {map_id} out of range [0, {0xF}]")

    @classmethod
    @gds_plugin_implementation
    def register_framing_plugin(cls):
        """Register this plugin as a framing implementation"""
        return cls


class SpacePacketUslpFramerDeframer(ChainedFramerDeframer):
    """Composite of the Space Packet and USLP framer/deframers"""

    @classmethod
    def get_composites(cls):
        """Return the chained framer/deframers, innermost first"""
        return [
            SpacePacketFramerDeframer,
            UslpFramerDeframer,
        ]

    @classmethod
    def get_name(cls):
        """Name of this implementation provided to CLI"""
        return "space-packet-uslp"

    @classmethod
    @gds_plugin_implementation
    def register_framing_plugin(cls):
        """Register this plugin as a framing implementation"""
        return cls
