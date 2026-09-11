"""CCSDS TC Transfer Frame (232.0-B-4) construction with Segment Headers and optional SDLS.

Ground-side mirror of `Svc/Ccsds/TcDeframer` (Segment Header mode), `Svc/Ccsds/Utils/SdlsTcAuthMask`
and `Svc/Ccsds/AesGcmDecryptor`. Every byte layout is pinned by DESIGN-final.md section 4.6 through
`check_vectors.py`.

Frame layout (FECF always present):

    clear: PrimHdr(5) | SH(1) | Space Packet portion | FECF(2)
    SDLS:  PrimHdr(5) | SH(1) | SPI(2) | IV(12) | ciphertext | MAC(16) | FECF(2)
"""

import struct
from typing import List, Optional, Sequence

import crcmod

from .aes_gcm import aes256_gcm_encrypt

TC_HEADER_SIZE = 5
SEGMENT_HEADER_SIZE = 1
TC_TRAILER_SIZE = 2
TC_MAX_FRAME_SIZE = 1024

SDLS_SPI_SIZE = 2
SDLS_IV_SIZE = 12
SDLS_MAC_SIZE = 16
SDLS_OVERHEAD = SDLS_SPI_SIZE + SDLS_IV_SIZE + SDLS_MAC_SIZE
SDLS_AAD_SIZE = 20

# Largest Space Packet portion per frame (DESIGN-final.md 4.3): 1016 clear, 986 with SDLS
MAX_PORTION_CLEAR = (
    TC_MAX_FRAME_SIZE - TC_HEADER_SIZE - SEGMENT_HEADER_SIZE - TC_TRAILER_SIZE
)
MAX_PORTION_SDLS = MAX_PORTION_CLEAR - SDLS_OVERHEAD

# Segment Header sequence flags (232.0-B-4 4.1.3.2.2), bits 7-6 of the octet
FLAG_CONTINUING = 0b00
FLAG_FIRST = 0b01
FLAG_LAST = 0b10
FLAG_UNSEGMENTED = 0b11

MAP_ID_MAX = 0x3F
SCID_MAX = 0x3FF
VCID_MAX = 0x3F
FSN_MODULUS = 256

# CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no final XOR (Svc/Ccsds/Utils/CRC16.hpp)
CRC16 = crcmod.mkCrcFun(0x11021, initCrc=0xFFFF, xorOut=0x0000, rev=False)


def segment_header(flags: int, map_id: int) -> int:
    if flags not in (FLAG_CONTINUING, FLAG_FIRST, FLAG_LAST, FLAG_UNSEGMENTED):
        raise ValueError(f"invalid sequence flags {flags}")
    if not 0 <= map_id <= MAP_ID_MAX:
        raise ValueError(f"MAP ID {map_id} out of range 0..{MAP_ID_MAX}")
    return (flags << 6) | map_id


def primary_header(
    scid: int, vcid: int, frame_length: int, fsn: int, control: bool = False
) -> bytes:
    """Type-BD (or Type-BC when `control`) TC primary header: TFVN 00, Bypass 1, Control flag."""
    if not 0 <= scid <= SCID_MAX:
        raise ValueError(f"SCID {scid} out of range")
    if not 0 <= vcid <= VCID_MAX:
        raise ValueError(f"VCID {vcid} out of range")
    if not TC_HEADER_SIZE + TC_TRAILER_SIZE <= frame_length <= TC_MAX_FRAME_SIZE:
        raise ValueError(f"frame length {frame_length} out of range")
    word1 = (1 << 13) | ((1 if control else 0) << 12) | (scid & SCID_MAX)
    word2 = ((vcid & VCID_MAX) << 10) | ((frame_length - 1) & 0x3FF)
    return struct.pack(">HHB", word1, word2, fsn % FSN_MODULUS)


def build_frame(
    scid: int, vcid: int, fsn: int, data_field: bytes, control: bool = False
) -> bytes:
    """Assemble header + data field + FECF; `data_field` already contains the SH for Type-BD frames."""
    length = TC_HEADER_SIZE + len(data_field) + TC_TRAILER_SIZE
    body = primary_header(scid, vcid, length, fsn, control) + data_field
    return body + struct.pack(">H", CRC16(body))


def control_frame(scid: int, vcid: int, fsn: int, fdu: bytes = b"\x00") -> bytes:
    """Type-BC control frame (232.0-B-4 4.1.3.3.3); FDU 0x00 = Unlock. Octet 5 is the FDU, not a SH."""
    return build_frame(scid, vcid, fsn, fdu, control=True)


def sdls_aad(vcid: int, spi: int, sh: Optional[int]) -> bytes:
    """Authentication mask output (Svc/Ccsds/Utils/SdlsTcAuthMask): VCID bits, SH (when present), SPI.

    19 octets without a Segment Header (baseline), 20 with one. The IV positions are masked to zero.
    """
    header = bytes([0, 0, (vcid & VCID_MAX) << 2, 0, 0])
    sh_part = b"" if sh is None else bytes([sh & 0xFF])
    return header + sh_part + struct.pack(">H", spi & 0xFFFF) + bytes(SDLS_IV_SIZE)


def sdls_iv(counter: int) -> bytes:
    return counter.to_bytes(SDLS_IV_SIZE, "big")


def sdls_protect(
    key: bytes, spi: int, iv: bytes, vcid: int, sh: int, portion: bytes
) -> bytes:
    """Return SPI | IV | ciphertext | MAC for one segment (232.0-B-4 SH authenticated through the AAD)."""
    aad = sdls_aad(vcid, spi, sh)
    if len(aad) != SDLS_AAD_SIZE:
        raise AssertionError(
            f"SDLS AAD must be {SDLS_AAD_SIZE} octets with a Segment Header"
        )
    ciphertext, mac = aes256_gcm_encrypt(key, iv, portion, aad)
    return struct.pack(">H", spi & 0xFFFF) + iv + ciphertext + mac


class Segment:
    """One Segment Header + Space Packet portion, before the frame is wrapped around it."""

    def __init__(self, flags: int, map_id: int, portion: bytes):
        self.flags = flags
        self.map_id = map_id
        self.portion = portion

    @property
    def sh(self) -> int:
        return segment_header(self.flags, self.map_id)


def segment_packet(packet: bytes, map_id: int, max_portion: int) -> List[Segment]:
    """Split one Space Packet into Segment portions (232.0-B-4 4.1.3.2.2.3).

    A packet that fits in one frame is UNSEGMENTED; otherwise FIRST, zero or more CONTINUING, LAST with
    `portion = min(remaining, max_portion)`.
    """
    if max_portion <= 0:
        raise ValueError("max_portion must be positive")
    if len(packet) == 0:
        raise ValueError("a Space Packet has at least the primary header")
    if len(packet) <= max_portion:
        return [Segment(FLAG_UNSEGMENTED, map_id, packet)]
    portions = [packet[i : i + max_portion] for i in range(0, len(packet), max_portion)]
    segments = [Segment(FLAG_FIRST, map_id, portions[0])]
    segments += [
        Segment(FLAG_CONTINUING, map_id, portion) for portion in portions[1:-1]
    ]
    segments.append(Segment(FLAG_LAST, map_id, portions[-1]))
    return segments


def frame_segment(
    scid: int,
    vcid: int,
    fsn: int,
    segment: Segment,
    sdls_key: Optional[bytes] = None,
    spi: int = 0,
    iv_counter: int = 0,
    sh_override: Optional[int] = None,
    corrupt_mac: bool = False,
    strip_sh: bool = False,
) -> bytes:
    """Wrap one Segment into a Type-BD frame.

    `sh_override` replaces the Segment Header octet *after* SDLS protection (the AAD authenticates the
    genuine SH, so flight must reject the frame). `corrupt_mac` flips the last MAC octet after protection
    and recomputes the FECF, so the frame passes the CRC check and fails at the SDLS gate. `strip_sh`
    omits the Segment Header (feature-off probe: a frame without SH delivered to a Segment-Header-mode
    deframer, or a Segment-Header frame delivered to a baseline deframer, depending on the deployment).
    """
    sh = segment.sh
    if sdls_key is None:
        secured = segment.portion
    else:
        secured = sdls_protect(
            sdls_key, spi, sdls_iv(iv_counter), vcid, sh, segment.portion
        )
        if corrupt_mac:
            secured = secured[:-1] + bytes([secured[-1] ^ 0x01])
    if sh_override is not None:
        sh = sh_override & 0xFF
    data_field = (b"" if strip_sh else bytes([sh])) + secured
    return build_frame(scid, vcid, fsn, data_field)


def frame_size_for_portion(portion_length: int, sdls: bool) -> int:
    return (
        TC_HEADER_SIZE
        + SEGMENT_HEADER_SIZE
        + (SDLS_OVERHEAD if sdls else 0)
        + portion_length
        + TC_TRAILER_SIZE
    )


def hexstr(data: Sequence[int]) -> str:
    return " ".join(f"{byte:02X}" for byte in data)
