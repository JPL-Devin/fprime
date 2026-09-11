"""Offline interoperability check of the tc-segment plugin against DESIGN-final.md section 4.6.

Run with `python -m tc_segment_plugin.check_vectors` (PYTHONPATH must contain the `test/int` directory).
Exits non-zero on the first mismatch. No network, no subprocess, no files written.
"""

import sys

from . import tc_frames as tf
from .aes_gcm import aes256_gcm_encrypt

KEY = bytes(range(0x40, 0x60))
KAT_IV = bytes(range(12))
KAT_PLAINTEXT = b"SDLS AES-256-GCM KAT payload"
KAT_CIPHERTEXT = bytes.fromhex(
    "6941202B57CD01F3EF20B5F495AA8FF6A38F6C5D4C3CCC2A824F89F2"
)

SCID = 0x044
VCID = 1
MAP = 0


def _h(text: str) -> bytes:
    return bytes.fromhex(text.replace(" ", ""))


def check(name: str, actual: bytes, expected: bytes) -> None:
    if actual != expected:
        print(
            f"FAIL {name}\n  expected {tf.hexstr(expected)}\n  actual   {tf.hexstr(actual)}"
        )
        sys.exit(1)
    print(f"ok   {name} ({len(actual)} octets)")


def check_kat_vectors() -> None:
    # 4.6.1: 19-octet AAD without Segment Header (feature-off identity)
    aad19 = tf.sdls_aad(vcid=5, spi=0x1234, sh=None)
    check(
        "4.6.1 AAD(19)",
        aad19,
        _h("00 00 14 00 00 12 34 00 00 00 00 00 00 00 00 00 00 00 00"),
    )
    ciphertext, mac = aes256_gcm_encrypt(KEY, KAT_IV, KAT_PLAINTEXT, aad19)
    check("4.6.1 ciphertext", ciphertext, KAT_CIPHERTEXT)
    check("4.6.1 MAC", mac, _h("C4 B0 91 03 7F A7 A4 AB D7 25 CB A2 E8 24 08 A6"))

    # 4.6.2: 20-octet AAD, VC 5, SPI 0x1234, MAP 1
    expected = {
        tf.FLAG_FIRST: ("41", "52 37 0D DF 09 C8 59 AA 82 CB 2A 99 FE 9B D6 08"),
        tf.FLAG_UNSEGMENTED: ("C1", "07 BE F4 29 81 64 EA 83 C0 73 BD 87 56 76 59 66"),
        tf.FLAG_CONTINUING: ("01", "78 F3 F1 24 4D 9E 00 3E 23 97 61 16 AA ED 11 BF"),
        tf.FLAG_LAST: ("81", "2D 7A 08 D2 C5 32 B3 17 61 2F F6 08 02 00 9E D1"),
    }
    for flags, (sh_hex, mac_hex) in expected.items():
        sh = tf.segment_header(flags, 1)
        check(f"4.6.2 SH={sh_hex}", bytes([sh]), _h(sh_hex))
        aad = tf.sdls_aad(vcid=5, spi=0x1234, sh=sh)
        check(
            f"4.6.2 AAD SH={sh_hex}",
            aad,
            _h("00 00 14 00 00") + bytes([sh]) + _h("12 34") + bytes(12),
        )
        _, mac = aes256_gcm_encrypt(KEY, KAT_IV, KAT_PLAINTEXT, aad)
        check(f"4.6.2 MAC SH={sh_hex}", mac, _h(mac_hex))


def check_clear_frames() -> None:
    # 4.6.3 (a): 36-octet packet segmented with a 12-octet portion, then an UNSEGMENTED packet
    packet = _h("18 0F C0 07 00 1D") + bytes(range(0x01, 0x1F))
    segments = tf.segment_packet(packet, MAP, max_portion=12)
    assert [s.flags for s in segments] == [
        tf.FLAG_FIRST,
        tf.FLAG_CONTINUING,
        tf.FLAG_LAST,
    ]
    frames = [
        tf.frame_segment(SCID, VCID, fsn, seg) for fsn, seg in enumerate(segments)
    ]
    check(
        "4.6.3a frame 1",
        frames[0],
        _h("20 44 04 13 00 40 18 0F C0 07 00 1D 01 02 03 04 05 06 78 66"),
    )
    check(
        "4.6.3a frame 2",
        frames[1],
        _h("20 44 04 13 01 00 07 08 09 0A 0B 0C 0D 0E 0F 10 11 12 2D 24"),
    )
    check(
        "4.6.3a frame 3",
        frames[2],
        _h("20 44 04 13 02 80 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 77 40"),
    )
    unsegmented = tf.segment_packet(
        _h("18 0F C0 08 00 05 AA BB CC DD EE FF"), MAP, max_portion=tf.MAX_PORTION_CLEAR
    )
    assert [s.flags for s in unsegmented] == [tf.FLAG_UNSEGMENTED]
    check(
        "4.6.3a frame 4",
        tf.frame_segment(SCID, VCID, 3, unsegmented[0]),
        _h("20 44 04 13 03 C0 18 0F C0 08 00 05 AA BB CC DD EE FF F6 1E"),
    )


def check_sdls_frames() -> None:
    # 4.6.3 (b): two-frame AES-256-GCM packet, SPI 1, IV = 00x11 || FSN, MAP 0
    packet = _h("18 0F C0 09 00 0D") + bytes(range(0x10, 0x1E))
    segments = tf.segment_packet(packet, MAP, max_portion=10)
    assert [s.flags for s in segments] == [tf.FLAG_FIRST, tf.FLAG_LAST]
    check(
        "4.6.3b S1 AAD",
        tf.sdls_aad(VCID, 1, segments[0].sh),
        _h("00 00 04 00 00 40 00 01") + bytes(12),
    )
    s1 = tf.frame_segment(
        SCID, VCID, 0x0A, segments[0], sdls_key=KEY, spi=1, iv_counter=0x0A
    )
    s2 = tf.frame_segment(
        SCID, VCID, 0x0B, segments[1], sdls_key=KEY, spi=1, iv_counter=0x0B
    )
    check(
        "4.6.3b S1",
        s1,
        _h(
            "20 44 04 2F 0A 40 00 01 00 00 00 00 00 00 00 00 00 00 00 0A 7D 19 F9 38 5F EB C6 DE F8 EB "
            "E4 F9 83 80 47 D7 A8 2F 21 77 85 1D 13 98 3E DF D1 A4"
        ),
    )
    check(
        "4.6.3b S2",
        s2,
        _h(
            "20 44 04 2F 0B 80 00 01 00 00 00 00 00 00 00 00 00 00 00 0B 8B D5 1B 18 17 9D C4 F6 CA C9 "
            "0D B9 34 63 93 A9 F9 B7 0A D2 19 37 3E 69 A4 16 6C D7"
        ),
    )
    # 4.6.5: S2 with the last MAC octet flipped and the FECF recomputed (MF8 mid-packet MAC failure)
    s2_bad = tf.frame_segment(
        SCID,
        VCID,
        0x0B,
        segments[1],
        sdls_key=KEY,
        spi=1,
        iv_counter=0x0B,
        corrupt_mac=True,
    )
    check(
        "4.6.5 S2'",
        s2_bad,
        _h(
            "20 44 04 2F 0B 80 00 01 00 00 00 00 00 00 00 00 00 00 00 0B 8B D5 1B 18 17 9D C4 F6 CA C9 "
            "0D B9 34 63 93 A9 F9 B7 0A D2 19 37 3E 69 A4 17 7C F6"
        ),
    )
    assert tf.CRC16(s2_bad[:-2]) == int.from_bytes(s2_bad[-2:], "big")
    # 4.6.3 (c): UNSEGMENTED packet on VC 0
    unsegmented = tf.segment_packet(
        _h("18 0F C0 08 00 05 AA BB CC DD EE FF"), MAP, max_portion=tf.MAX_PORTION_SDLS
    )
    check(
        "4.6.3c VC0 frame",
        tf.frame_segment(
            SCID, 0, 0x0C, unsegmented[0], sdls_key=KEY, spi=1, iv_counter=0x0C
        ),
        _h(
            "20 44 00 31 0C C0 00 01 00 00 00 00 00 00 00 00 00 00 00 0C 6C 5F 2E D2 92 03 8B 71 4F 64 2E 63 "
            "D6 5C 2D 0B C7 75 62 1D 58 84 C1 71 59 B2 6B 49 85 88"
        ),
    )
    # SPI 0x1234 variant of the 4.6.2 FIRST vector carried in a frame (I18 negative path uses this SPI)
    first = tf.Segment(tf.FLAG_FIRST, 1, KAT_PLAINTEXT)
    frame = tf.frame_segment(
        SCID,
        5,
        0,
        first,
        sdls_key=KEY,
        spi=0x1234,
        iv_counter=int.from_bytes(KAT_IV, "big"),
    )
    check("4.6.2 FIRST in frame: SH|SPI|IV", frame[5:20], _h("41 12 34") + KAT_IV)
    check("4.6.2 FIRST in frame: ciphertext", frame[20:48], KAT_CIPHERTEXT)
    check(
        "4.6.2 FIRST in frame: MAC",
        frame[48:64],
        _h("52 37 0D DF 09 C8 59 AA 82 CB 2A 99 FE 9B D6 08"),
    )


def check_control_frame() -> None:
    # 4.6.4: Type-BC Unlock frame; octet 5 is the FDU, never a Segment Header
    check(
        "4.6.4 Type-BC", tf.control_frame(SCID, VCID, 0), _h("30 44 04 07 00 00 F6 93")
    )


def check_segmentation_bounds() -> None:
    assert tf.MAX_PORTION_CLEAR == 1016
    assert tf.MAX_PORTION_SDLS == 986
    for size, max_portion in ((1016, tf.MAX_PORTION_CLEAR), (986, tf.MAX_PORTION_SDLS)):
        assert len(tf.segment_packet(bytes(size), MAP, max_portion)) == 1
        two = tf.segment_packet(bytes(size + 1), MAP, max_portion)
        assert [s.flags for s in two] == [tf.FLAG_FIRST, tf.FLAG_LAST] and len(
            two[1].portion
        ) == 1
    three_k = tf.segment_packet(bytes(3000), MAP, tf.MAX_PORTION_SDLS)
    assert [s.flags for s in three_k] == [
        tf.FLAG_FIRST,
        tf.FLAG_CONTINUING,
        tf.FLAG_CONTINUING,
        tf.FLAG_LAST,
    ]
    largest = tf.frame_segment(
        SCID, VCID, 0, tf.Segment(tf.FLAG_FIRST, MAP, bytes(986)), sdls_key=KEY, spi=1
    )
    assert len(largest) == tf.TC_MAX_FRAME_SIZE
    print("ok   segmentation bounds (1016 clear / 986 SDLS, 1024-octet frame ceiling)")


def main() -> int:
    check_kat_vectors()
    check_clear_frames()
    check_sdls_frames()
    check_control_frame()
    check_segmentation_bounds()
    print("All DESIGN-final.md 4.6 vectors reproduced")
    return 0


if __name__ == "__main__":
    sys.exit(main())
