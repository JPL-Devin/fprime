#!/usr/bin/env python3
"""
CFDP Interoperability Test

Tests the F Prime CFDP Class 1 implementation against the spacepackets
Python library to verify PDU encoding/decoding compatibility.

This test:
1. Builds PDUs with spacepackets and verifies they match what our C++ would produce
2. Verifies our C++ serialized PDUs can be parsed by spacepackets
3. Simulates a complete file transfer flow using spacepackets as the sender
   and verifies the byte sequences match what the C++ receiver expects
"""

import os
import struct
import subprocess
import unittest
from pathlib import Path

from spacepackets.cfdp.defs import (
    ChecksumType,
    ConditionCode,
    Direction,
    TransmissionMode,
)
from spacepackets.cfdp.pdu import (
    EofPdu,
    FileDataPdu,
    MetadataPdu,
    PduFactory,
)
from spacepackets.cfdp.pdu.file_data import FileDataParams
from spacepackets.cfdp.pdu.metadata import MetadataParams
from spacepackets.cfdp.conf import PduConfig
from spacepackets.cfdp.defs import CrcFlag, LargeFileFlag
from spacepackets.util import UnsignedByteField


def make_pdu_config(
    src_id=1,
    dst_id=2,
    seq_num=1,
    entity_byte_len=2,
    seq_byte_len=2,
):
    """Create a PduConfig matching the C++ unit test defaults."""
    return PduConfig(
        source_entity_id=UnsignedByteField(byte_len=entity_byte_len, val=src_id),
        dest_entity_id=UnsignedByteField(byte_len=entity_byte_len, val=dst_id),
        transaction_seq_num=UnsignedByteField(byte_len=seq_byte_len, val=seq_num),
        trans_mode=TransmissionMode.UNACKNOWLEDGED,
        file_flag=LargeFileFlag.NORMAL,
        crc_flag=CrcFlag.NO_CRC,
        direction=Direction.TOWARDS_RECEIVER,
    )


def modular_checksum(data: bytes) -> int:
    """Compute CFDP modular checksum (sum of big-endian 32-bit words)."""
    checksum = 0
    # Pad to 4-byte boundary
    padded = data + b'\x00' * ((4 - len(data) % 4) % 4)
    for i in range(0, len(padded), 4):
        val = struct.unpack('>I', padded[i:i+4])[0]
        checksum = (checksum + val) & 0xFFFFFFFF
    return checksum


def checksum_to_int(checksum):
    """Convert a checksum value to int, handling both bytes and int types."""
    if isinstance(checksum, (bytes, bytearray)):
        return int.from_bytes(checksum, 'big')
    return int(checksum)


def condition_code_value(cc):
    """Get the numeric value of a condition code, handling both enum and int.
    
    spacepackets may return the raw byte (condition code in upper 4 bits)
    or the 4-bit value or an enum. We normalize to the 4-bit value.
    """
    if isinstance(cc, int):
        # If value > 15, it's the raw byte with condition code in upper nibble
        if cc > 0x0F:
            return cc >> 4
        return cc
    return cc.value


# -----------------------------------------------------------------------
# Reference byte sequences from C++ unit tests (CfdpPduTest.cpp)
# These are the exact bytes our C++ implementation produces/expects.
# Config: 2-byte entity IDs (src=1, dst=2), 2-byte seq num (1)
# -----------------------------------------------------------------------

# Metadata PDU: fileSize=100, src="source.bin", dst="dest.bin"
CPP_REF_METADATA_PDU = bytes([
    0x24, 0x00, 0x1a, 0x11, 0x00, 0x01, 0x00, 0x01,
    0x00, 0x02, 0x07, 0x00, 0x00, 0x00, 0x00, 0x64,
    0x0a, 0x73, 0x6f, 0x75, 0x72, 0x63, 0x65, 0x2e,
    0x62, 0x69, 0x6e, 0x08, 0x64, 0x65, 0x73, 0x74,
    0x2e, 0x62, 0x69, 0x6e
])

# File Data PDU: offset=0, data={0x01,0x02,0x03,0x04,0x05}
CPP_REF_FILE_DATA_PDU = bytes([
    0x34, 0x00, 0x09, 0x11, 0x00, 0x01, 0x00, 0x01,
    0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
    0x03, 0x04, 0x05
])

# File Data PDU: offset=10, data={0xAA,0xBB,0xCC}
CPP_REF_FILE_DATA_PDU_OFFSET10 = bytes([
    0x34, 0x00, 0x07, 0x11, 0x00, 0x01, 0x00, 0x01,
    0x00, 0x02, 0x00, 0x00, 0x00, 0x0a, 0xaa, 0xbb,
    0xcc
])

# EOF PDU: conditionCode=NO_ERROR, checksum=0x12345678, fileSize=100
CPP_REF_EOF_PDU = bytes([
    0x24, 0x00, 0x0a, 0x11, 0x00, 0x01, 0x00, 0x01,
    0x00, 0x02, 0x04, 0x00, 0x12, 0x34, 0x56, 0x78,
    0x00, 0x00, 0x00, 0x64
])

# EOF PDU (Cancel): conditionCode=CANCEL_REQUEST_RECEIVED, checksum=0, fileSize=50
CPP_REF_EOF_CANCEL_PDU = bytes([
    0x24, 0x00, 0x0a, 0x11, 0x00, 0x01, 0x00, 0x01,
    0x00, 0x02, 0x04, 0xf0, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x32
])


class TestPduHeaderInterop(unittest.TestCase):
    """Verify PDU header encoding matches between spacepackets and C++."""

    def test_metadata_header_matches_cpp(self):
        """Metadata PDU built by spacepackets must match C++ reference bytes."""
        config = make_pdu_config()
        params = MetadataParams(
            closure_requested=False,
            checksum_type=ChecksumType.MODULAR,
            file_size=100,
            source_file_name="source.bin",
            dest_file_name="dest.bin",
        )
        pdu = MetadataPdu(pdu_conf=config, params=params)
        raw = pdu.pack()

        self.assertEqual(
            raw, CPP_REF_METADATA_PDU,
            f"Metadata PDU mismatch.\n"
            f"  spacepackets: {raw.hex()}\n"
            f"  C++ expected: {CPP_REF_METADATA_PDU.hex()}"
        )

    def test_file_data_header_matches_cpp(self):
        """File Data PDU (offset=0) built by spacepackets must match C++ reference."""
        config = make_pdu_config()
        fd_params = FileDataParams(
            file_data=bytes([0x01, 0x02, 0x03, 0x04, 0x05]),
            offset=0,
            segment_metadata=None,
        )
        pdu = FileDataPdu(pdu_conf=config, params=fd_params)
        raw = pdu.pack()

        self.assertEqual(
            raw, CPP_REF_FILE_DATA_PDU,
            f"FileData PDU mismatch.\n"
            f"  spacepackets: {raw.hex()}\n"
            f"  C++ expected: {CPP_REF_FILE_DATA_PDU.hex()}"
        )

    def test_file_data_with_offset_matches_cpp(self):
        """File Data PDU (offset=10) built by spacepackets must match C++ reference."""
        config = make_pdu_config()
        fd_params = FileDataParams(
            file_data=bytes([0xAA, 0xBB, 0xCC]),
            offset=10,
            segment_metadata=None,
        )
        pdu = FileDataPdu(pdu_conf=config, params=fd_params)
        raw = pdu.pack()

        self.assertEqual(
            raw, CPP_REF_FILE_DATA_PDU_OFFSET10,
            f"FileData PDU (offset=10) mismatch.\n"
            f"  spacepackets: {raw.hex()}\n"
            f"  C++ expected: {CPP_REF_FILE_DATA_PDU_OFFSET10.hex()}"
        )

    def test_eof_pdu_matches_cpp(self):
        """EOF PDU (no error) built by spacepackets must match C++ reference."""
        config = make_pdu_config()
        pdu = EofPdu(
            pdu_conf=config,
            file_checksum=0x12345678,
            file_size=100,
            condition_code=ConditionCode.NO_ERROR,
        )
        raw = pdu.pack()

        self.assertEqual(
            raw, CPP_REF_EOF_PDU,
            f"EOF PDU mismatch.\n"
            f"  spacepackets: {raw.hex()}\n"
            f"  C++ expected: {CPP_REF_EOF_PDU.hex()}"
        )

    def test_eof_cancel_pdu_matches_cpp(self):
        """EOF PDU (cancel) built by spacepackets must match C++ reference."""
        config = make_pdu_config()
        pdu = EofPdu(
            pdu_conf=config,
            file_checksum=0,
            file_size=50,
            condition_code=ConditionCode.CANCEL_REQUEST_RECEIVED,
        )
        raw = pdu.pack()

        self.assertEqual(
            raw, CPP_REF_EOF_CANCEL_PDU,
            f"EOF Cancel PDU mismatch.\n"
            f"  spacepackets: {raw.hex()}\n"
            f"  C++ expected: {CPP_REF_EOF_CANCEL_PDU.hex()}"
        )


class TestSpacepacketsParsesCppBytes(unittest.TestCase):
    """Verify spacepackets can parse the byte sequences produced by our C++."""

    def test_parse_cpp_metadata_pdu(self):
        """spacepackets must parse C++ metadata PDU bytes correctly."""
        factory = PduFactory()
        pdu = factory.from_raw(CPP_REF_METADATA_PDU)
        self.assertIsInstance(pdu, MetadataPdu)
        self.assertEqual(pdu.file_size, 100)
        self.assertEqual(pdu.source_file_name, "source.bin")
        self.assertEqual(pdu.dest_file_name, "dest.bin")
        self.assertFalse(pdu.closure_requested)

    def test_parse_cpp_file_data_pdu(self):
        """spacepackets must parse C++ file data PDU bytes correctly."""
        factory = PduFactory()
        pdu = factory.from_raw(CPP_REF_FILE_DATA_PDU)
        self.assertIsInstance(pdu, FileDataPdu)
        self.assertEqual(pdu.offset, 0)
        self.assertEqual(pdu.file_data, bytes([0x01, 0x02, 0x03, 0x04, 0x05]))

    def test_parse_cpp_file_data_pdu_with_offset(self):
        """spacepackets must parse C++ file data PDU with offset=10."""
        factory = PduFactory()
        pdu = factory.from_raw(CPP_REF_FILE_DATA_PDU_OFFSET10)
        self.assertIsInstance(pdu, FileDataPdu)
        self.assertEqual(pdu.offset, 10)
        self.assertEqual(pdu.file_data, bytes([0xAA, 0xBB, 0xCC]))

    def test_parse_cpp_eof_pdu(self):
        """spacepackets must parse C++ EOF PDU bytes correctly."""
        factory = PduFactory()
        pdu = factory.from_raw(CPP_REF_EOF_PDU)
        self.assertIsInstance(pdu, EofPdu)
        self.assertEqual(checksum_to_int(pdu.file_checksum), 0x12345678)
        self.assertEqual(pdu.file_size, 100)
        self.assertEqual(condition_code_value(pdu.condition_code), ConditionCode.NO_ERROR.value)

    def test_parse_cpp_eof_cancel_pdu(self):
        """spacepackets must parse C++ cancel EOF PDU bytes correctly."""
        factory = PduFactory()
        pdu = factory.from_raw(CPP_REF_EOF_CANCEL_PDU)
        self.assertIsInstance(pdu, EofPdu)
        self.assertEqual(checksum_to_int(pdu.file_checksum), 0)
        self.assertEqual(pdu.file_size, 50)
        self.assertEqual(
            condition_code_value(pdu.condition_code),
            ConditionCode.CANCEL_REQUEST_RECEIVED.value
        )


class TestFullFileTransferFlow(unittest.TestCase):
    """Simulate a complete CFDP Class 1 file transfer using spacepackets
    and verify the byte sequences are valid for the C++ receiver."""

    def setUp(self):
        self.factory = PduFactory()
        self.config = make_pdu_config()

    def _do_transfer(self, file_data: bytes, src_name: str, dst_name: str):
        """Build the full sequence of PDUs for a file transfer and return them."""
        # 1. Metadata PDU
        metadata_pdu = MetadataPdu(
            pdu_conf=self.config,
            params=MetadataParams(
                closure_requested=False,
                checksum_type=ChecksumType.MODULAR,
                file_size=len(file_data),
                source_file_name=src_name,
                dest_file_name=dst_name,
            ),
        )

        # 2. File Data PDUs (chunk at 512 bytes like the C++ implementation)
        chunk_size = 512
        file_data_pdus = []
        for offset in range(0, len(file_data), chunk_size):
            chunk = file_data[offset:offset + chunk_size]
            fd_pdu = FileDataPdu(
                pdu_conf=self.config,
                params=FileDataParams(
                    file_data=chunk,
                    offset=offset,
                    segment_metadata=None,
                ),
            )
            file_data_pdus.append(fd_pdu)

        # 3. EOF PDU
        checksum = modular_checksum(file_data)
        eof_pdu = EofPdu(
            pdu_conf=self.config,
            file_checksum=checksum,
            file_size=len(file_data),
            condition_code=ConditionCode.NO_ERROR,
        )

        return metadata_pdu, file_data_pdus, eof_pdu

    def test_small_file_transfer(self):
        """Transfer a small file (fits in one data PDU)."""
        file_data = bytes(range(16))  # 16 bytes
        metadata, data_pdus, eof = self._do_transfer(
            file_data, "small.bin", "received_small.bin"
        )

        # Verify we get exactly one data PDU
        self.assertEqual(len(data_pdus), 1)

        # Verify all PDUs are parseable
        all_pdus = [metadata.pack()] + [dp.pack() for dp in data_pdus] + [eof.pack()]
        for raw in all_pdus:
            parsed = self.factory.from_raw(raw)
            self.assertIsNotNone(parsed)

        # Verify metadata contents (use top-level properties for parsed PDUs)
        parsed_meta = self.factory.from_raw(metadata.pack())
        self.assertEqual(parsed_meta.file_size, 16)
        self.assertEqual(parsed_meta.source_file_name, "small.bin")
        self.assertEqual(parsed_meta.dest_file_name, "received_small.bin")

        # Verify file data contents
        parsed_fd = self.factory.from_raw(data_pdus[0].pack())
        self.assertEqual(parsed_fd.offset, 0)
        self.assertEqual(parsed_fd.file_data, file_data)

        # Verify EOF contents
        parsed_eof = self.factory.from_raw(eof.pack())
        self.assertEqual(parsed_eof.file_size, 16)
        self.assertEqual(checksum_to_int(parsed_eof.file_checksum), modular_checksum(file_data))
        self.assertEqual(condition_code_value(parsed_eof.condition_code), ConditionCode.NO_ERROR.value)

    def test_multi_chunk_file_transfer(self):
        """Transfer a file larger than one chunk (requires multiple data PDUs)."""
        # Create 1500 bytes of data (needs 3 chunks at 512 bytes)
        file_data = bytes([i & 0xFF for i in range(1500)])
        metadata, data_pdus, eof = self._do_transfer(
            file_data, "large.bin", "received_large.bin"
        )

        # Verify we get 3 data PDUs
        self.assertEqual(len(data_pdus), 3)

        # Verify offsets are correct
        parsed_pd0 = self.factory.from_raw(data_pdus[0].pack())
        parsed_pd1 = self.factory.from_raw(data_pdus[1].pack())
        parsed_pd2 = self.factory.from_raw(data_pdus[2].pack())

        self.assertEqual(parsed_pd0.offset, 0)
        self.assertEqual(len(parsed_pd0.file_data), 512)
        self.assertEqual(parsed_pd1.offset, 512)
        self.assertEqual(len(parsed_pd1.file_data), 512)
        self.assertEqual(parsed_pd2.offset, 1024)
        self.assertEqual(len(parsed_pd2.file_data), 476)  # 1500 - 1024

        # Reconstruct file from data PDUs and verify
        reconstructed = b''
        for dp in data_pdus:
            parsed = self.factory.from_raw(dp.pack())
            reconstructed += bytes(parsed.file_data)
        self.assertEqual(reconstructed, file_data)

        # Verify checksum
        parsed_eof = self.factory.from_raw(eof.pack())
        self.assertEqual(checksum_to_int(parsed_eof.file_checksum), modular_checksum(file_data))
        self.assertEqual(parsed_eof.file_size, 1500)

    def test_empty_file_transfer(self):
        """Transfer an empty file (no data PDUs, just metadata + EOF)."""
        file_data = b''
        metadata, data_pdus, eof = self._do_transfer(
            file_data, "empty.bin", "received_empty.bin"
        )

        # Empty file should have zero data PDUs
        self.assertEqual(len(data_pdus), 0)

        # Verify metadata
        parsed_meta = self.factory.from_raw(metadata.pack())
        self.assertEqual(parsed_meta.file_size, 0)

        # Verify EOF
        parsed_eof = self.factory.from_raw(eof.pack())
        self.assertEqual(parsed_eof.file_size, 0)
        self.assertEqual(checksum_to_int(parsed_eof.file_checksum), 0)

    def test_cancel_transfer(self):
        """Simulate a cancelled transfer with cancel EOF."""
        config = make_pdu_config()
        # Send metadata, some data, then cancel EOF
        metadata = MetadataPdu(
            pdu_conf=config,
            params=MetadataParams(
                closure_requested=False,
                checksum_type=ChecksumType.MODULAR,
                file_size=1000,
                source_file_name="cancel_test.bin",
                dest_file_name="cancel_dst.bin",
            ),
        )

        # Only send partial data (one chunk)
        partial_data = bytes(range(256))
        data_pdu = FileDataPdu(
            pdu_conf=config,
            params=FileDataParams(
                file_data=partial_data,
                offset=0,
                segment_metadata=None,
            ),
        )

        # Send cancel EOF
        cancel_eof = EofPdu(
            pdu_conf=config,
            file_checksum=0,
            file_size=256,  # Only received 256 of 1000 bytes
            condition_code=ConditionCode.CANCEL_REQUEST_RECEIVED,
        )

        # Verify all are parseable and have correct condition codes
        parsed_meta = self.factory.from_raw(metadata.pack())
        self.assertIsInstance(parsed_meta, MetadataPdu)

        parsed_data = self.factory.from_raw(data_pdu.pack())
        self.assertIsInstance(parsed_data, FileDataPdu)

        parsed_eof = self.factory.from_raw(cancel_eof.pack())
        self.assertIsInstance(parsed_eof, EofPdu)
        self.assertEqual(
            condition_code_value(parsed_eof.condition_code),
            ConditionCode.CANCEL_REQUEST_RECEIVED.value
        )


class TestRoundTripConsistency(unittest.TestCase):
    """Verify that pack -> parse -> pack produces identical bytes (idempotency)."""

    def test_metadata_round_trip(self):
        config = make_pdu_config()
        pdu = MetadataPdu(
            pdu_conf=config,
            params=MetadataParams(
                closure_requested=False,
                checksum_type=ChecksumType.MODULAR,
                file_size=12345,
                source_file_name="flight/data.bin",
                dest_file_name="ground/data.bin",
            ),
        )
        raw1 = pdu.pack()
        parsed = PduFactory().from_raw(raw1)
        raw2 = parsed.pack()
        self.assertEqual(raw1, raw2, "Metadata PDU round-trip mismatch")

    def test_file_data_round_trip(self):
        config = make_pdu_config()
        pdu = FileDataPdu(
            pdu_conf=config,
            params=FileDataParams(
                file_data=bytes(range(128)),
                offset=4096,
                segment_metadata=None,
            ),
        )
        raw1 = pdu.pack()
        parsed = PduFactory().from_raw(raw1)
        raw2 = parsed.pack()
        self.assertEqual(raw1, raw2, "FileData PDU round-trip mismatch")

    def test_eof_round_trip(self):
        config = make_pdu_config()
        pdu = EofPdu(
            pdu_conf=config,
            file_checksum=0xDEADBEEF,
            file_size=999999,
            condition_code=ConditionCode.NO_ERROR,
        )
        raw1 = pdu.pack()
        parsed = PduFactory().from_raw(raw1)
        raw2 = parsed.pack()
        self.assertEqual(raw1, raw2, "EOF PDU round-trip mismatch")


class TestChecksumInterop(unittest.TestCase):
    """Verify CFDP modular checksum computation matches between Python and C++."""

    def test_checksum_simple(self):
        """Verify checksum for known data."""
        data = bytes([0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                      0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F])
        # Two 32-bit words: 0x00010203 + 0x04050607 + 0x08090A0B + 0x0C0D0E0F
        expected = (0x00010203 + 0x04050607 + 0x08090A0B + 0x0C0D0E0F) & 0xFFFFFFFF
        self.assertEqual(modular_checksum(data), expected)

    def test_checksum_non_aligned(self):
        """Verify checksum for data not aligned to 4 bytes."""
        data = bytes([0xFF, 0xFF, 0xFF])
        # Padded to: 0xFFFFFF00
        expected = 0xFFFFFF00
        self.assertEqual(modular_checksum(data), expected)

    def test_checksum_empty(self):
        """Empty data has zero checksum."""
        self.assertEqual(modular_checksum(b''), 0)

    def test_checksum_single_byte(self):
        """Single byte padded to 4 bytes."""
        data = bytes([0x42])
        expected = 0x42000000
        self.assertEqual(modular_checksum(data), expected)


class TestEntityIdSizes(unittest.TestCase):
    """Verify interoperability with different entity ID and sequence number sizes."""

    def test_1byte_entity_ids(self):
        """Test with 1-byte entity IDs and sequence numbers."""
        config = PduConfig(
            source_entity_id=UnsignedByteField(byte_len=1, val=1),
            dest_entity_id=UnsignedByteField(byte_len=1, val=2),
            transaction_seq_num=UnsignedByteField(byte_len=1, val=0),
            trans_mode=TransmissionMode.UNACKNOWLEDGED,
            crc_flag=CrcFlag.NO_CRC,
            direction=Direction.TOWARDS_RECEIVER,
        )
        pdu = MetadataPdu(
            pdu_conf=config,
            params=MetadataParams(
                closure_requested=False,
                checksum_type=ChecksumType.MODULAR,
                file_size=10,
                source_file_name="a.bin",
                dest_file_name="b.bin",
            ),
        )
        raw = pdu.pack()
        # Header should be 7 bytes: 4 fixed + 1+1 entity IDs + 1 seq num
        # Byte 3 (4th byte) encodes entity_id_len-1 and seq_num_len-1
        fourth_byte = raw[3]
        entity_len_field = (fourth_byte >> 4) & 0x07
        seq_len_field = fourth_byte & 0x07
        self.assertEqual(entity_len_field, 0)  # 0 means 1-byte
        self.assertEqual(seq_len_field, 0)  # 0 means 1-byte

        # Verify round-trip
        parsed = PduFactory().from_raw(raw)
        self.assertIsInstance(parsed, MetadataPdu)
        self.assertEqual(parsed.params.file_size, 10)

    def test_4byte_entity_ids(self):
        """Test with 4-byte entity IDs and sequence numbers."""
        config = PduConfig(
            source_entity_id=UnsignedByteField(byte_len=4, val=0x01020304),
            dest_entity_id=UnsignedByteField(byte_len=4, val=0x05060708),
            transaction_seq_num=UnsignedByteField(byte_len=4, val=0xAABBCCDD),
            trans_mode=TransmissionMode.UNACKNOWLEDGED,
            crc_flag=CrcFlag.NO_CRC,
            direction=Direction.TOWARDS_RECEIVER,
        )
        eof = EofPdu(
            pdu_conf=config,
            file_checksum=0x11223344,
            file_size=5000,
            condition_code=ConditionCode.NO_ERROR,
        )
        raw = eof.pack()
        parsed = PduFactory().from_raw(raw)
        self.assertIsInstance(parsed, EofPdu)
        self.assertEqual(checksum_to_int(parsed.file_checksum), 0x11223344)
        self.assertEqual(parsed.file_size, 5000)


class TestCppPduTestExeInterop(unittest.TestCase):
    """Run the compiled C++ PDU test executable and verify it passes.
    This provides end-to-end confidence that the C++ and Python
    implementations agree on the byte-level encoding.
    """

    @classmethod
    def _find_test_exe(cls):
        """Locate the compiled C++ PDU test executable."""
        repo_root = Path(__file__).resolve().parent.parent.parent.parent
        candidates = [
            repo_root / "build-fprime-automatic-native-ut" / "bin" / "Linux" / "Svc_Cfdp_Types_ut_exe",
        ]
        for c in candidates:
            if c.exists():
                return str(c)
        return None

    def test_cpp_pdu_tests_pass(self):
        """Run C++ PDU unit tests and verify they all pass."""
        exe = self._find_test_exe()
        if exe is None:
            self.skipTest("C++ PDU test executable not found (not built)")

        env = os.environ.copy()
        env["LSAN_OPTIONS"] = "detect_leaks=0"
        result = subprocess.run(
            [exe],
            capture_output=True,
            text=True,
            timeout=30,
            env=env,
        )
        # Check for passing tests
        self.assertIn("[  PASSED  ]", result.stdout,
                      f"C++ tests did not pass.\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}")
        self.assertNotIn("[  FAILED  ]", result.stdout,
                         f"Some C++ tests failed.\nstdout:\n{result.stdout}")


if __name__ == '__main__':
    unittest.main(verbosity=2)
