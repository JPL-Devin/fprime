#!/usr/bin/env python3
"""Unit tests for the ComLogger decoder tool."""

import json
import struct
import subprocess
import sys
import tempfile
import os
import unittest
from pathlib import Path

from comlogger_decoder import (
    Dictionary,
    decode_com_file,
    decode_packet,
    parse_time,
    deserialize_value,
    decode_telemetry_packet,
    decode_event_packet,
    decode_packetized_tlm_packet,
    FW_PACKET_TELEM,
    FW_PACKET_LOG,
    FW_PACKET_PACKETIZED_TLM,
    FW_PACKET_COMMAND,
    FW_PACKET_FILE,
    FW_PACKET_DP,
    PACKET_DESCRIPTOR_SIZE,
    CHAN_ID_SIZE,
    EVENT_ID_SIZE,
    TIME_SIZE,
    TLM_PACKETIZE_ID_SIZE,
    PACKET_TYPE_NAMES,
)


def make_time_bytes(time_base=0, time_context=0, seconds=100, useconds=500000):
    """Create serialized Fw::Time bytes."""
    return struct.pack(">HBI I", time_base, time_context, seconds, useconds)


def make_telemetry_entry(
    chan_id,
    value_bytes,
    time_base=0,
    time_context=0,
    seconds=100,
    useconds=500000,
):
    """Create a complete ComBuffer payload for a telemetry packet."""
    payload = struct.pack(">H", FW_PACKET_TELEM)  # descriptor
    payload += struct.pack(">I", chan_id)  # channel ID
    payload += make_time_bytes(time_base, time_context, seconds, useconds)
    payload += value_bytes
    return payload


def make_event_entry(
    event_id,
    arg_bytes,
    time_base=0,
    time_context=0,
    seconds=200,
    useconds=0,
):
    """Create a complete ComBuffer payload for an event packet."""
    payload = struct.pack(">H", FW_PACKET_LOG)
    payload += struct.pack(">I", event_id)
    payload += make_time_bytes(time_base, time_context, seconds, useconds)
    payload += arg_bytes
    return payload


def make_packetized_tlm_entry(
    pkt_id,
    value_bytes,
    time_base=0,
    time_context=0,
    seconds=300,
    useconds=0,
):
    """Create a complete ComBuffer payload for a packetized telemetry packet."""
    payload = struct.pack(">H", FW_PACKET_PACKETIZED_TLM)
    payload += struct.pack(">H", pkt_id)  # packet ID (U16)
    payload += make_time_bytes(time_base, time_context, seconds, useconds)
    payload += value_bytes
    return payload


def make_com_file_data(entries, store_buffer_length=True):
    """Create binary .com file data from a list of payload byte arrays."""
    data = b""
    for entry in entries:
        if store_buffer_length:
            data += struct.pack(">H", len(entry))
        data += entry
    return data


def make_minimal_dictionary(
    channels=None, events=None, type_definitions=None, telemetry_packet_sets=None
):
    """Create a minimal FPP JSON dictionary for testing."""
    d = {
        "telemetryChannels": channels or [],
        "events": events or [],
        "typeDefinitions": type_definitions or [],
    }
    if telemetry_packet_sets is not None:
        d["telemetryPacketSets"] = telemetry_packet_sets
    return d


def write_temp_dict(dict_data):
    """Write dictionary data to a temp JSON file, return path."""
    fd, path = tempfile.mkstemp(suffix=".json")
    with os.fdopen(fd, "w") as f:
        json.dump(dict_data, f)
    return path


def write_temp_bin(binary_data):
    """Write binary data to a temp .com file, return path."""
    fd, path = tempfile.mkstemp(suffix=".com")
    with os.fdopen(fd, "wb") as f:
        f.write(binary_data)
    return path


class TestParseTime(unittest.TestCase):
    """Tests for the parse_time function."""

    def test_basic_time(self):
        data = make_time_bytes(time_base=1, time_context=2, seconds=100, useconds=500000)
        result, offset = parse_time(data, 0)
        self.assertEqual(result["timeBase"], 1)
        self.assertEqual(result["timeContext"], 2)
        self.assertEqual(result["seconds"], 100)
        self.assertEqual(result["useconds"], 500000)
        self.assertEqual(offset, TIME_SIZE)

    def test_time_with_offset(self):
        prefix = b"\x00\x00\x00"  # 3 bytes of padding
        data = prefix + make_time_bytes(seconds=42, useconds=123456)
        result, offset = parse_time(data, 3)
        self.assertEqual(result["seconds"], 42)
        self.assertEqual(result["useconds"], 123456)
        self.assertEqual(offset, 3 + TIME_SIZE)

    def test_zero_time(self):
        data = make_time_bytes(time_base=0, time_context=0, seconds=0, useconds=0)
        result, offset = parse_time(data, 0)
        self.assertEqual(result["timeBase"], 0)
        self.assertEqual(result["timeContext"], 0)
        self.assertEqual(result["seconds"], 0)
        self.assertEqual(result["useconds"], 0)

    def test_max_values(self):
        data = make_time_bytes(
            time_base=0xFFFF, time_context=0xFF, seconds=0xFFFFFFFF, useconds=0xFFFFFFFF
        )
        result, _ = parse_time(data, 0)
        self.assertEqual(result["timeBase"], 0xFFFF)
        self.assertEqual(result["timeContext"], 0xFF)
        self.assertEqual(result["seconds"], 0xFFFFFFFF)
        self.assertEqual(result["useconds"], 0xFFFFFFFF)


class TestDictionary(unittest.TestCase):
    """Tests for the Dictionary class."""

    def test_load_empty_dictionary(self):
        dict_data = make_minimal_dictionary()
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            self.assertIsNone(d.get_channel(0))
            self.assertIsNone(d.get_event(0))
            self.assertIsNone(d.get_type_definition("foo"))
        finally:
            os.unlink(path)

    def test_channel_lookup(self):
        channels = [
            {"id": 1, "name": "chan1", "type": {"kind": "integer", "size": 32, "signed": False}},
            {"id": 2, "name": "chan2", "type": {"kind": "float", "size": 32}},
        ]
        dict_data = make_minimal_dictionary(channels=channels)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            ch = d.get_channel(1)
            self.assertIsNotNone(ch)
            self.assertEqual(ch["name"], "chan1")
            self.assertIsNone(d.get_channel(999))
        finally:
            os.unlink(path)

    def test_event_lookup(self):
        events = [
            {
                "id": 10,
                "name": "TestEvent",
                "severity": "WARNING_HI",
                "format": "Something happened: {}",
                "formalParams": [
                    {"name": "code", "type": {"kind": "integer", "size": 32, "signed": False}}
                ],
            }
        ]
        dict_data = make_minimal_dictionary(events=events)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            ev = d.get_event(10)
            self.assertIsNotNone(ev)
            self.assertEqual(ev["name"], "TestEvent")
            self.assertIsNone(d.get_event(999))
        finally:
            os.unlink(path)

    def test_type_definition_lookup(self):
        type_defs = [
            {
                "qualifiedName": "MyModule.MyEnum",
                "kind": "enum",
                "representationType": {"kind": "integer", "size": 32, "signed": True},
                "enumeratedConstants": [
                    {"name": "VALUE_A", "value": 0},
                    {"name": "VALUE_B", "value": 1},
                ],
            }
        ]
        dict_data = make_minimal_dictionary(type_definitions=type_defs)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            td = d.get_type_definition("MyModule.MyEnum")
            self.assertIsNotNone(td)
            self.assertEqual(td["kind"], "enum")
            self.assertIsNone(d.get_type_definition("Nonexistent"))
        finally:
            os.unlink(path)

    def test_raw_property(self):
        dict_data = make_minimal_dictionary()
        dict_data["extra"] = "value"
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            self.assertEqual(d.raw["extra"], "value")
        finally:
            os.unlink(path)


class TestDeserializeValue(unittest.TestCase):
    """Tests for the deserialize_value function."""

    def setUp(self):
        dict_data = make_minimal_dictionary()
        self._dict_path = write_temp_dict(dict_data)
        self.dictionary = Dictionary(self._dict_path)

    def tearDown(self):
        os.unlink(self._dict_path)

    def test_u8(self):
        data = struct.pack(">B", 42)
        val, off = deserialize_value(data, 0, {"kind": "integer", "size": 8, "signed": False}, self.dictionary)
        self.assertEqual(val, 42)
        self.assertEqual(off, 1)

    def test_i8(self):
        data = struct.pack(">b", -10)
        val, off = deserialize_value(data, 0, {"kind": "integer", "size": 8, "signed": True}, self.dictionary)
        self.assertEqual(val, -10)
        self.assertEqual(off, 1)

    def test_u16(self):
        data = struct.pack(">H", 1000)
        val, off = deserialize_value(data, 0, {"kind": "integer", "size": 16, "signed": False}, self.dictionary)
        self.assertEqual(val, 1000)
        self.assertEqual(off, 2)

    def test_i16(self):
        data = struct.pack(">h", -500)
        val, off = deserialize_value(data, 0, {"kind": "integer", "size": 16, "signed": True}, self.dictionary)
        self.assertEqual(val, -500)
        self.assertEqual(off, 2)

    def test_u32(self):
        data = struct.pack(">I", 100000)
        val, off = deserialize_value(data, 0, {"kind": "integer", "size": 32, "signed": False}, self.dictionary)
        self.assertEqual(val, 100000)
        self.assertEqual(off, 4)

    def test_i32(self):
        data = struct.pack(">i", -100000)
        val, off = deserialize_value(data, 0, {"kind": "integer", "size": 32, "signed": True}, self.dictionary)
        self.assertEqual(val, -100000)
        self.assertEqual(off, 4)

    def test_u64(self):
        data = struct.pack(">Q", 2**60)
        val, off = deserialize_value(data, 0, {"kind": "integer", "size": 64, "signed": False}, self.dictionary)
        self.assertEqual(val, 2**60)
        self.assertEqual(off, 8)

    def test_i64(self):
        data = struct.pack(">q", -(2**60))
        val, off = deserialize_value(data, 0, {"kind": "integer", "size": 64, "signed": True}, self.dictionary)
        self.assertEqual(val, -(2**60))
        self.assertEqual(off, 8)

    def test_float32(self):
        data = struct.pack(">f", 3.14)
        val, off = deserialize_value(data, 0, {"kind": "float", "size": 32}, self.dictionary)
        self.assertAlmostEqual(val, 3.14, places=5)
        self.assertEqual(off, 4)

    def test_float64(self):
        data = struct.pack(">d", 3.141592653589793)
        val, off = deserialize_value(data, 0, {"kind": "float", "size": 64}, self.dictionary)
        self.assertAlmostEqual(val, 3.141592653589793, places=10)
        self.assertEqual(off, 8)

    def test_bool_true(self):
        data = struct.pack(">B", 1)
        val, off = deserialize_value(data, 0, {"kind": "bool"}, self.dictionary)
        self.assertTrue(val)
        self.assertEqual(off, 1)

    def test_bool_false(self):
        data = struct.pack(">B", 0)
        val, off = deserialize_value(data, 0, {"kind": "bool"}, self.dictionary)
        self.assertFalse(val)
        self.assertEqual(off, 1)

    def test_string(self):
        test_str = "hello"
        data = struct.pack(">H", len(test_str)) + test_str.encode("utf-8")
        val, off = deserialize_value(data, 0, {"kind": "string"}, self.dictionary)
        self.assertEqual(val, "hello")
        self.assertEqual(off, 2 + len(test_str))

    def test_empty_string(self):
        data = struct.pack(">H", 0)
        val, off = deserialize_value(data, 0, {"kind": "string"}, self.dictionary)
        self.assertEqual(val, "")
        self.assertEqual(off, 2)

    def test_enum_type(self):
        type_defs = [
            {
                "qualifiedName": "TestModule.Status",
                "kind": "enum",
                "representationType": {"kind": "integer", "size": 32, "signed": True},
                "enumeratedConstants": [
                    {"name": "OK", "value": 0},
                    {"name": "ERROR", "value": 1},
                    {"name": "TIMEOUT", "value": 2},
                ],
            }
        ]
        dict_data = make_minimal_dictionary(type_definitions=type_defs)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            data = struct.pack(">i", 1)  # ERROR
            val, off = deserialize_value(
                data, 0, {"kind": "qualifiedIdentifier", "name": "TestModule.Status"}, d
            )
            self.assertEqual(val, "ERROR")
            self.assertEqual(off, 4)
        finally:
            os.unlink(path)

    def test_enum_unknown_value(self):
        type_defs = [
            {
                "qualifiedName": "TestModule.Status",
                "kind": "enum",
                "representationType": {"kind": "integer", "size": 32, "signed": True},
                "enumeratedConstants": [
                    {"name": "OK", "value": 0},
                ],
            }
        ]
        dict_data = make_minimal_dictionary(type_definitions=type_defs)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            data = struct.pack(">i", 99)  # not in enum
            val, off = deserialize_value(
                data, 0, {"kind": "qualifiedIdentifier", "name": "TestModule.Status"}, d
            )
            self.assertEqual(val, 99)  # raw numeric value
            self.assertEqual(off, 4)
        finally:
            os.unlink(path)

    def test_struct_type(self):
        type_defs = [
            {
                "qualifiedName": "TestModule.Point",
                "kind": "struct",
                "members": {
                    "x": {"type": {"kind": "float", "size": 32}, "index": 0},
                    "y": {"type": {"kind": "float", "size": 32}, "index": 1},
                },
            }
        ]
        dict_data = make_minimal_dictionary(type_definitions=type_defs)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            data = struct.pack(">ff", 1.5, 2.5)
            val, off = deserialize_value(
                data, 0, {"kind": "qualifiedIdentifier", "name": "TestModule.Point"}, d
            )
            self.assertAlmostEqual(val["x"], 1.5)
            self.assertAlmostEqual(val["y"], 2.5)
            self.assertEqual(off, 8)
        finally:
            os.unlink(path)

    def test_struct_type_with_array_member(self):
        """Test struct with a member that has size > 1 (array member per FPP spec)."""
        type_defs = [
            {
                "qualifiedName": "M1.S",
                "kind": "struct",
                "members": {
                    "w": {"type": {"kind": "integer", "size": 32, "signed": False}, "index": 0, "size": 3},
                    "x": {"type": {"kind": "integer", "size": 32, "signed": False}, "index": 1},
                    "y": {"type": {"kind": "float", "size": 32}, "index": 2},
                },
            }
        ]
        dict_data = make_minimal_dictionary(type_definitions=type_defs)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            # w=[10,20,30], x=42, y=3.14
            data = struct.pack(">IIIIf", 10, 20, 30, 42, 3.14)
            val, off = deserialize_value(
                data, 0, {"kind": "qualifiedIdentifier", "name": "M1.S"}, d
            )
            self.assertEqual(val["w"], [10, 20, 30])
            self.assertEqual(val["x"], 42)
            self.assertAlmostEqual(val["y"], 3.14, places=5)
            self.assertEqual(off, 20)  # 3*4 + 4 + 4
        finally:
            os.unlink(path)

    def test_struct_type_with_size_one_member(self):
        """Test struct member with size=1 produces a single-element array per FPP spec."""
        type_defs = [
            {
                "qualifiedName": "TestModule.Single",
                "kind": "struct",
                "members": {
                    "a": {"type": {"kind": "integer", "size": 32, "signed": False}, "index": 0, "size": 1},
                    "b": {"type": {"kind": "integer", "size": 32, "signed": False}, "index": 1},
                },
            }
        ]
        dict_data = make_minimal_dictionary(type_definitions=type_defs)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            data = struct.pack(">II", 42, 99)
            val, off = deserialize_value(
                data, 0, {"kind": "qualifiedIdentifier", "name": "TestModule.Single"}, d
            )
            # size=1 should produce a single-element list, not a scalar
            self.assertEqual(val["a"], [42])
            # no size field => scalar
            self.assertEqual(val["b"], 99)
            self.assertEqual(off, 8)
        finally:
            os.unlink(path)

    def test_struct_type_index_ordering(self):
        """Test that struct members are deserialized in index order, not dict key order."""
        type_defs = [
            {
                "qualifiedName": "TestModule.Ordered",
                "kind": "struct",
                "members": {
                    "b": {"type": {"kind": "integer", "size": 16, "signed": False}, "index": 1},
                    "a": {"type": {"kind": "integer", "size": 16, "signed": False}, "index": 0},
                },
            }
        ]
        dict_data = make_minimal_dictionary(type_definitions=type_defs)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            # Binary order: a first (index 0), then b (index 1)
            data = struct.pack(">HH", 111, 222)
            val, off = deserialize_value(
                data, 0, {"kind": "qualifiedIdentifier", "name": "TestModule.Ordered"}, d
            )
            self.assertEqual(val["a"], 111)
            self.assertEqual(val["b"], 222)
            self.assertEqual(off, 4)
        finally:
            os.unlink(path)

    def test_array_type(self):
        type_defs = [
            {
                "qualifiedName": "TestModule.Vec3",
                "kind": "array",
                "size": 3,
                "elementType": {"kind": "float", "size": 32},
            }
        ]
        dict_data = make_minimal_dictionary(type_definitions=type_defs)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            data = struct.pack(">fff", 1.0, 2.0, 3.0)
            val, off = deserialize_value(
                data, 0, {"kind": "qualifiedIdentifier", "name": "TestModule.Vec3"}, d
            )
            self.assertEqual(len(val), 3)
            self.assertAlmostEqual(val[0], 1.0)
            self.assertAlmostEqual(val[1], 2.0)
            self.assertAlmostEqual(val[2], 3.0)
            self.assertEqual(off, 12)
        finally:
            os.unlink(path)

    def test_alias_type(self):
        type_defs = [
            {
                "qualifiedName": "TestModule.MyU32",
                "kind": "alias",
                "type": {"kind": "integer", "size": 32, "signed": False},
            }
        ]
        dict_data = make_minimal_dictionary(type_definitions=type_defs)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            data = struct.pack(">I", 12345)
            val, off = deserialize_value(
                data, 0, {"kind": "qualifiedIdentifier", "name": "TestModule.MyU32"}, d
            )
            self.assertEqual(val, 12345)
            self.assertEqual(off, 4)
        finally:
            os.unlink(path)

    def test_unresolved_qualified_identifier(self):
        data = b"\x01\x02\x03\x04"
        val, off = deserialize_value(
            data, 0, {"kind": "qualifiedIdentifier", "name": "NonExistent.Type"}, self.dictionary
        )
        # Should return hex of remaining data
        self.assertEqual(val, "01020304")
        self.assertEqual(off, len(data))

    def test_fallback_unknown_kind(self):
        data = b"\xAB\xCD"
        val, off = deserialize_value(data, 0, {"kind": "unknownKind"}, self.dictionary)
        self.assertEqual(val, "abcd")
        self.assertEqual(off, len(data))

    def test_deserialize_with_offset(self):
        prefix = b"\x00\x00"  # 2 bytes padding
        data = prefix + struct.pack(">I", 42)
        val, off = deserialize_value(data, 2, {"kind": "integer", "size": 32, "signed": False}, self.dictionary)
        self.assertEqual(val, 42)
        self.assertEqual(off, 6)


class TestDecodeTelemetryPacket(unittest.TestCase):
    """Tests for decoding telemetry packets."""

    def test_u32_channel(self):
        channels = [
            {"id": 5, "name": "cpuUsage", "type": {"kind": "integer", "size": 32, "signed": False}}
        ]
        dict_data = make_minimal_dictionary(channels=channels)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            value_bytes = struct.pack(">I", 75)
            payload = make_telemetry_entry(5, value_bytes, seconds=100, useconds=500000)
            result = decode_packet(payload, d)

            self.assertEqual(result["type"], "FW_PACKET_TELEM")
            self.assertEqual(result["channelId"], 5)
            self.assertEqual(result["channelName"], "cpuUsage")
            self.assertEqual(result["value"], 75)
            self.assertEqual(result["time"]["seconds"], 100)
            self.assertEqual(result["time"]["useconds"], 500000)
        finally:
            os.unlink(path)

    def test_float_channel(self):
        channels = [
            {"id": 10, "name": "temperature", "type": {"kind": "float", "size": 32}}
        ]
        dict_data = make_minimal_dictionary(channels=channels)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            value_bytes = struct.pack(">f", 23.5)
            payload = make_telemetry_entry(10, value_bytes)
            result = decode_packet(payload, d)

            self.assertEqual(result["type"], "FW_PACKET_TELEM")
            self.assertEqual(result["channelName"], "temperature")
            self.assertAlmostEqual(result["value"], 23.5)
        finally:
            os.unlink(path)

    def test_unknown_channel(self):
        dict_data = make_minimal_dictionary()
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            value_bytes = struct.pack(">I", 999)
            payload = make_telemetry_entry(99, value_bytes)
            result = decode_packet(payload, d)

            self.assertEqual(result["type"], "FW_PACKET_TELEM")
            self.assertEqual(result["channelName"], "UNKNOWN_CHANNEL_99")
            # Value should be hex of remaining bytes
            self.assertIsInstance(result["value"], str)
        finally:
            os.unlink(path)

    def test_bool_channel(self):
        channels = [
            {"id": 20, "name": "isActive", "type": {"kind": "bool"}}
        ]
        dict_data = make_minimal_dictionary(channels=channels)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            value_bytes = struct.pack(">B", 1)
            payload = make_telemetry_entry(20, value_bytes)
            result = decode_packet(payload, d)

            self.assertEqual(result["channelName"], "isActive")
            self.assertTrue(result["value"])
        finally:
            os.unlink(path)

    def test_string_channel(self):
        channels = [
            {"id": 30, "name": "statusMsg", "type": {"kind": "string"}}
        ]
        dict_data = make_minimal_dictionary(channels=channels)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            test_str = "nominal"
            value_bytes = struct.pack(">H", len(test_str)) + test_str.encode("utf-8")
            payload = make_telemetry_entry(30, value_bytes)
            result = decode_packet(payload, d)

            self.assertEqual(result["channelName"], "statusMsg")
            self.assertEqual(result["value"], "nominal")
        finally:
            os.unlink(path)

    def test_enum_channel(self):
        type_defs = [
            {
                "qualifiedName": "Fw.Health",
                "kind": "enum",
                "representationType": {"kind": "integer", "size": 32, "signed": True},
                "enumeratedConstants": [
                    {"name": "HEALTHY", "value": 0},
                    {"name": "DEGRADED", "value": 1},
                    {"name": "FAILED", "value": 2},
                ],
            }
        ]
        channels = [
            {"id": 40, "name": "systemHealth", "type": {"kind": "qualifiedIdentifier", "name": "Fw.Health"}}
        ]
        dict_data = make_minimal_dictionary(channels=channels, type_definitions=type_defs)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            value_bytes = struct.pack(">i", 1)  # DEGRADED
            payload = make_telemetry_entry(40, value_bytes)
            result = decode_packet(payload, d)

            self.assertEqual(result["channelName"], "systemHealth")
            self.assertEqual(result["value"], "DEGRADED")
        finally:
            os.unlink(path)


class TestDecodeEventPacket(unittest.TestCase):
    """Tests for decoding event packets."""

    def test_event_no_args(self):
        events = [
            {
                "id": 100,
                "name": "SystemStarted",
                "severity": "ACTIVITY_HI",
                "format": "System started",
                "formalParams": [],
            }
        ]
        dict_data = make_minimal_dictionary(events=events)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            payload = make_event_entry(100, b"", seconds=200)
            result = decode_packet(payload, d)

            self.assertEqual(result["type"], "FW_PACKET_LOG")
            self.assertEqual(result["eventId"], 100)
            self.assertEqual(result["eventName"], "SystemStarted")
            self.assertEqual(result["severity"], "ACTIVITY_HI")
            self.assertEqual(result["args"], [])
            self.assertEqual(result["time"]["seconds"], 200)
        finally:
            os.unlink(path)

    def test_event_with_u32_arg(self):
        events = [
            {
                "id": 101,
                "name": "ErrorOccurred",
                "severity": "WARNING_HI",
                "format": "Error code: {}",
                "formalParams": [
                    {"name": "errorCode", "type": {"kind": "integer", "size": 32, "signed": False}}
                ],
            }
        ]
        dict_data = make_minimal_dictionary(events=events)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            arg_bytes = struct.pack(">I", 42)
            payload = make_event_entry(101, arg_bytes)
            result = decode_packet(payload, d)

            self.assertEqual(result["eventName"], "ErrorOccurred")
            self.assertEqual(len(result["args"]), 1)
            self.assertEqual(result["args"][0]["name"], "errorCode")
            self.assertEqual(result["args"][0]["value"], 42)
        finally:
            os.unlink(path)

    def test_event_with_multiple_args(self):
        events = [
            {
                "id": 102,
                "name": "FileOpened",
                "severity": "DIAGNOSTIC",
                "format": "Opened file {} with size {}",
                "formalParams": [
                    {"name": "fileName", "type": {"kind": "string"}},
                    {"name": "fileSize", "type": {"kind": "integer", "size": 32, "signed": False}},
                ],
            }
        ]
        dict_data = make_minimal_dictionary(events=events)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            filename = "test.dat"
            arg_bytes = struct.pack(">H", len(filename)) + filename.encode("utf-8")
            arg_bytes += struct.pack(">I", 1024)
            payload = make_event_entry(102, arg_bytes)
            result = decode_packet(payload, d)

            self.assertEqual(result["eventName"], "FileOpened")
            self.assertEqual(len(result["args"]), 2)
            self.assertEqual(result["args"][0]["name"], "fileName")
            self.assertEqual(result["args"][0]["value"], "test.dat")
            self.assertEqual(result["args"][1]["name"], "fileSize")
            self.assertEqual(result["args"][1]["value"], 1024)
        finally:
            os.unlink(path)

    def test_unknown_event(self):
        dict_data = make_minimal_dictionary()
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            payload = make_event_entry(999, b"\x01\x02\x03\x04")
            result = decode_packet(payload, d)

            self.assertEqual(result["type"], "FW_PACKET_LOG")
            self.assertEqual(result["eventName"], "UNKNOWN_EVENT_999")
            # args should be hex string of remaining data
            self.assertIsInstance(result["args"], str)
        finally:
            os.unlink(path)

    def test_event_with_enum_arg(self):
        type_defs = [
            {
                "qualifiedName": "Svc.CmdResult",
                "kind": "enum",
                "representationType": {"kind": "integer", "size": 32, "signed": True},
                "enumeratedConstants": [
                    {"name": "OK", "value": 0},
                    {"name": "FAIL", "value": 1},
                ],
            }
        ]
        events = [
            {
                "id": 103,
                "name": "CmdComplete",
                "severity": "ACTIVITY_LO",
                "format": "Command completed: {}",
                "formalParams": [
                    {"name": "result", "type": {"kind": "qualifiedIdentifier", "name": "Svc.CmdResult"}},
                ],
            }
        ]
        dict_data = make_minimal_dictionary(events=events, type_definitions=type_defs)
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            arg_bytes = struct.pack(">i", 0)  # OK
            payload = make_event_entry(103, arg_bytes)
            result = decode_packet(payload, d)

            self.assertEqual(result["args"][0]["value"], "OK")
        finally:
            os.unlink(path)


class TestDecodePacketizedTlm(unittest.TestCase):
    """Tests for decoding packetized telemetry packets."""

    def test_basic_packetized_tlm(self):
        dict_data = make_minimal_dictionary()
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            value_bytes = struct.pack(">I", 42)
            payload = make_packetized_tlm_entry(1, value_bytes)
            result = decode_packet(payload, d)

            self.assertEqual(result["type"], "FW_PACKET_PACKETIZED_TLM")
            self.assertEqual(result["packetId"], 1)
            self.assertIn("rawData", result)
            self.assertEqual(result["time"]["seconds"], 300)
        finally:
            os.unlink(path)

    def test_packetized_tlm_with_packet_set(self):
        channels = [
            {"id": 1, "name": "sensor1.temp", "type": {"kind": "float", "size": 32}},
            {"id": 2, "name": "sensor1.pressure", "type": {"kind": "float", "size": 32}},
        ]
        packet_sets = [
            {
                "members": [
                    {
                        "id": 10,
                        "name": "SensorPacket",
                        "members": ["sensor1.temp", "sensor1.pressure"],
                    }
                ]
            }
        ]
        dict_data = make_minimal_dictionary(
            channels=channels, telemetry_packet_sets=packet_sets
        )
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            value_bytes = struct.pack(">ff", 25.0, 101.3)
            payload = make_packetized_tlm_entry(10, value_bytes)
            result = decode_packet(payload, d)

            self.assertEqual(result["type"], "FW_PACKET_PACKETIZED_TLM")
            self.assertEqual(result["packetName"], "SensorPacket")
            self.assertEqual(len(result["channels"]), 2)
            self.assertEqual(result["channels"][0]["name"], "sensor1.temp")
            self.assertAlmostEqual(result["channels"][0]["value"], 25.0)
            self.assertEqual(result["channels"][1]["name"], "sensor1.pressure")
            self.assertAlmostEqual(result["channels"][1]["value"], 101.3, places=1)
        finally:
            os.unlink(path)


class TestDecodePacket(unittest.TestCase):
    """Tests for the decode_packet function."""

    def test_payload_too_short(self):
        dict_data = make_minimal_dictionary()
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            result = decode_packet(b"\x00", d)
            self.assertEqual(result["type"], "UNKNOWN")
            self.assertIn("error", result)
        finally:
            os.unlink(path)

    def test_empty_payload(self):
        dict_data = make_minimal_dictionary()
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            result = decode_packet(b"", d)
            self.assertEqual(result["type"], "UNKNOWN")
        finally:
            os.unlink(path)

    def test_command_packet(self):
        dict_data = make_minimal_dictionary()
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            payload = struct.pack(">H", FW_PACKET_COMMAND) + b"\x01\x02\x03"
            result = decode_packet(payload, d)
            self.assertEqual(result["type"], "FW_PACKET_COMMAND")
            self.assertEqual(result["descriptor"], FW_PACKET_COMMAND)
            self.assertIn("raw", result)
        finally:
            os.unlink(path)

    def test_file_packet(self):
        dict_data = make_minimal_dictionary()
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            payload = struct.pack(">H", FW_PACKET_FILE) + b"\xDE\xAD"
            result = decode_packet(payload, d)
            self.assertEqual(result["type"], "FW_PACKET_FILE")
        finally:
            os.unlink(path)

    def test_dp_packet(self):
        dict_data = make_minimal_dictionary()
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            payload = struct.pack(">H", FW_PACKET_DP) + b"\xBE\xEF"
            result = decode_packet(payload, d)
            self.assertEqual(result["type"], "FW_PACKET_DP")
        finally:
            os.unlink(path)

    def test_unknown_descriptor(self):
        dict_data = make_minimal_dictionary()
        path = write_temp_dict(dict_data)
        try:
            d = Dictionary(path)
            payload = struct.pack(">H", 0x00FF) + b"\x01\x02"
            result = decode_packet(payload, d)
            self.assertIn("UNKNOWN", result["type"])
            self.assertEqual(result["descriptor"], 0x00FF)
        finally:
            os.unlink(path)


class TestDecodeComFile(unittest.TestCase):
    """Tests for the decode_com_file function with store_buffer_length=True (default)."""

    def test_single_telemetry_entry(self):
        channels = [
            {"id": 1, "name": "counter", "type": {"kind": "integer", "size": 32, "signed": False}}
        ]
        dict_data = make_minimal_dictionary(channels=channels)
        dict_path = write_temp_dict(dict_data)
        try:
            d = Dictionary(dict_path)
            value_bytes = struct.pack(">I", 42)
            entry = make_telemetry_entry(1, value_bytes)
            file_data = make_com_file_data([entry])
            bin_path = write_temp_bin(file_data)
            try:
                results = decode_com_file(bin_path, d)
                self.assertEqual(len(results), 1)
                self.assertEqual(results[0]["type"], "FW_PACKET_TELEM")
                self.assertEqual(results[0]["channelName"], "counter")
                self.assertEqual(results[0]["value"], 42)
                self.assertEqual(results[0]["index"], 0)
            finally:
                os.unlink(bin_path)
        finally:
            os.unlink(dict_path)

    def test_multiple_entries(self):
        channels = [
            {"id": 1, "name": "ch1", "type": {"kind": "integer", "size": 32, "signed": False}},
            {"id": 2, "name": "ch2", "type": {"kind": "float", "size": 32}},
        ]
        events = [
            {
                "id": 10,
                "name": "ev1",
                "severity": "ACTIVITY_LO",
                "format": "",
                "formalParams": [],
            }
        ]
        dict_data = make_minimal_dictionary(channels=channels, events=events)
        dict_path = write_temp_dict(dict_data)
        try:
            d = Dictionary(dict_path)
            entry1 = make_telemetry_entry(1, struct.pack(">I", 100), seconds=10)
            entry2 = make_telemetry_entry(2, struct.pack(">f", 3.14), seconds=20)
            entry3 = make_event_entry(10, b"", seconds=30)

            file_data = make_com_file_data([entry1, entry2, entry3])
            bin_path = write_temp_bin(file_data)
            try:
                results = decode_com_file(bin_path, d)
                self.assertEqual(len(results), 3)
                self.assertEqual(results[0]["type"], "FW_PACKET_TELEM")
                self.assertEqual(results[0]["value"], 100)
                self.assertEqual(results[0]["index"], 0)

                self.assertEqual(results[1]["type"], "FW_PACKET_TELEM")
                self.assertAlmostEqual(results[1]["value"], 3.14, places=5)
                self.assertEqual(results[1]["index"], 1)

                self.assertEqual(results[2]["type"], "FW_PACKET_LOG")
                self.assertEqual(results[2]["eventName"], "ev1")
                self.assertEqual(results[2]["index"], 2)
            finally:
                os.unlink(bin_path)
        finally:
            os.unlink(dict_path)

    def test_empty_file(self):
        dict_data = make_minimal_dictionary()
        dict_path = write_temp_dict(dict_data)
        try:
            d = Dictionary(dict_path)
            bin_path = write_temp_bin(b"")
            try:
                results = decode_com_file(bin_path, d)
                self.assertEqual(len(results), 0)
            finally:
                os.unlink(bin_path)
        finally:
            os.unlink(dict_path)

    def test_truncated_entry_size(self):
        """Test handling of a file that ends with only 1 byte (incomplete U16 size)."""
        dict_data = make_minimal_dictionary()
        dict_path = write_temp_dict(dict_data)
        try:
            d = Dictionary(dict_path)
            bin_path = write_temp_bin(b"\x00")  # only 1 byte, can't read U16
            try:
                results = decode_com_file(bin_path, d)
                self.assertEqual(len(results), 0)
            finally:
                os.unlink(bin_path)
        finally:
            os.unlink(dict_path)

    def test_entry_size_exceeds_data(self):
        """Test handling when the declared entry size exceeds remaining file data."""
        dict_data = make_minimal_dictionary()
        dict_path = write_temp_dict(dict_data)
        try:
            d = Dictionary(dict_path)
            # Declare size of 100 bytes but only provide 4 bytes
            file_data = struct.pack(">H", 100) + b"\x01\x02\x03\x04"
            bin_path = write_temp_bin(file_data)
            try:
                results = decode_com_file(bin_path, d)
                self.assertEqual(len(results), 1)
                self.assertIn("error", results[0])
            finally:
                os.unlink(bin_path)
        finally:
            os.unlink(dict_path)

    def test_zero_size_entry(self):
        """Test that a zero-size entry stops parsing."""
        dict_data = make_minimal_dictionary()
        dict_path = write_temp_dict(dict_data)
        try:
            d = Dictionary(dict_path)
            file_data = struct.pack(">H", 0)  # zero size
            bin_path = write_temp_bin(file_data)
            try:
                results = decode_com_file(bin_path, d)
                self.assertEqual(len(results), 0)
            finally:
                os.unlink(bin_path)
        finally:
            os.unlink(dict_path)

    def test_mixed_packet_types(self):
        """Test a file containing telemetry, event, and an unknown packet type."""
        channels = [
            {"id": 1, "name": "ch1", "type": {"kind": "integer", "size": 16, "signed": False}}
        ]
        events = [
            {
                "id": 50,
                "name": "TestEvt",
                "severity": "WARNING_LO",
                "format": "val={}",
                "formalParams": [
                    {"name": "val", "type": {"kind": "integer", "size": 8, "signed": False}}
                ],
            }
        ]
        dict_data = make_minimal_dictionary(channels=channels, events=events)
        dict_path = write_temp_dict(dict_data)
        try:
            d = Dictionary(dict_path)
            # Telemetry entry
            tlm_entry = make_telemetry_entry(1, struct.pack(">H", 42))
            # Event entry
            evt_entry = make_event_entry(50, struct.pack(">B", 7))
            # Unknown packet type
            unk_entry = struct.pack(">H", 0x00FF) + b"\xAB\xCD"

            file_data = make_com_file_data([tlm_entry, evt_entry, unk_entry])
            bin_path = write_temp_bin(file_data)
            try:
                results = decode_com_file(bin_path, d)
                self.assertEqual(len(results), 3)
                self.assertEqual(results[0]["type"], "FW_PACKET_TELEM")
                self.assertEqual(results[0]["value"], 42)
                self.assertEqual(results[1]["type"], "FW_PACKET_LOG")
                self.assertEqual(results[1]["args"][0]["value"], 7)
                self.assertIn("UNKNOWN", results[2]["type"])
            finally:
                os.unlink(bin_path)
        finally:
            os.unlink(dict_path)


class TestDecodeComFileFixedSize(unittest.TestCase):
    """Tests for decode_com_file with store_buffer_length=False (fixed-size entries)."""

    def test_fixed_size_entries(self):
        channels = [
            {"id": 1, "name": "ch1", "type": {"kind": "integer", "size": 32, "signed": False}}
        ]
        dict_data = make_minimal_dictionary(channels=channels)
        dict_path = write_temp_dict(dict_data)
        try:
            d = Dictionary(dict_path)
            # Each entry is: 2 (descriptor) + 4 (chan_id) + 11 (time) + 4 (U32 value) = 21 bytes
            entry = make_telemetry_entry(1, struct.pack(">I", 100))
            fixed_size = len(entry)
            self.assertEqual(fixed_size, 21)

            # No length prefix
            file_data = make_com_file_data([entry, entry], store_buffer_length=False)
            bin_path = write_temp_bin(file_data)
            try:
                results = decode_com_file(bin_path, d, store_buffer_length=False, fixed_size=fixed_size)
                self.assertEqual(len(results), 2)
                self.assertEqual(results[0]["value"], 100)
                self.assertEqual(results[1]["value"], 100)
            finally:
                os.unlink(bin_path)
        finally:
            os.unlink(dict_path)

    def test_fixed_size_no_size_raises(self):
        dict_data = make_minimal_dictionary()
        dict_path = write_temp_dict(dict_data)
        try:
            d = Dictionary(dict_path)
            bin_path = write_temp_bin(b"\x00" * 20)
            try:
                with self.assertRaises(ValueError):
                    decode_com_file(bin_path, d, store_buffer_length=False, fixed_size=None)
            finally:
                os.unlink(bin_path)
        finally:
            os.unlink(dict_path)

    def test_fixed_size_partial_entry_ignored(self):
        """If remaining data is less than fixed_size, it should stop cleanly."""
        channels = [
            {"id": 1, "name": "ch1", "type": {"kind": "integer", "size": 32, "signed": False}}
        ]
        dict_data = make_minimal_dictionary(channels=channels)
        dict_path = write_temp_dict(dict_data)
        try:
            d = Dictionary(dict_path)
            entry = make_telemetry_entry(1, struct.pack(">I", 42))
            fixed_size = len(entry)

            # Two complete entries plus some trailing bytes (less than one entry)
            file_data = entry + entry + b"\x00\x01\x02"
            bin_path = write_temp_bin(file_data)
            try:
                results = decode_com_file(bin_path, d, store_buffer_length=False, fixed_size=fixed_size)
                self.assertEqual(len(results), 2)
            finally:
                os.unlink(bin_path)
        finally:
            os.unlink(dict_path)


class TestEndToEnd(unittest.TestCase):
    """End-to-end tests simulating realistic .com file scenarios."""

    def test_realistic_mixed_file(self):
        """Simulate a realistic .com file with multiple telemetry and event entries."""
        type_defs = [
            {
                "qualifiedName": "Fw.On",
                "kind": "enum",
                "representationType": {"kind": "integer", "size": 32, "signed": True},
                "enumeratedConstants": [
                    {"name": "OFF", "value": 0},
                    {"name": "ON", "value": 1},
                ],
            }
        ]
        channels = [
            {"id": 0x0100, "name": "Ref.cmdCount", "type": {"kind": "integer", "size": 32, "signed": False}},
            {"id": 0x0101, "name": "Ref.errCount", "type": {"kind": "integer", "size": 32, "signed": False}},
            {"id": 0x0200, "name": "Ref.heaterState", "type": {"kind": "qualifiedIdentifier", "name": "Fw.On"}},
        ]
        events = [
            {
                "id": 0x0500,
                "name": "Ref.CmdDispatched",
                "severity": "ACTIVITY_HI",
                "format": "Opcode {} dispatched",
                "formalParams": [
                    {"name": "opcode", "type": {"kind": "integer", "size": 32, "signed": False}},
                ],
            },
            {
                "id": 0x0501,
                "name": "Ref.SystemReady",
                "severity": "ACTIVITY_HI",
                "format": "System ready",
                "formalParams": [],
            },
        ]
        dict_data = make_minimal_dictionary(
            channels=channels, events=events, type_definitions=type_defs
        )
        dict_path = write_temp_dict(dict_data)
        try:
            d = Dictionary(dict_path)
            entries = []

            # Telemetry: cmdCount = 5
            entries.append(
                make_telemetry_entry(0x0100, struct.pack(">I", 5), seconds=1000, useconds=0)
            )
            # Telemetry: errCount = 0
            entries.append(
                make_telemetry_entry(0x0101, struct.pack(">I", 0), seconds=1000, useconds=100000)
            )
            # Telemetry: heaterState = ON (enum value 1)
            entries.append(
                make_telemetry_entry(0x0200, struct.pack(">i", 1), seconds=1001, useconds=0)
            )
            # Event: CmdDispatched with opcode=0x1234
            entries.append(
                make_event_entry(0x0500, struct.pack(">I", 0x1234), seconds=1001, useconds=500000)
            )
            # Event: SystemReady (no args)
            entries.append(
                make_event_entry(0x0501, b"", seconds=1002, useconds=0)
            )

            file_data = make_com_file_data(entries)
            bin_path = write_temp_bin(file_data)
            try:
                results = decode_com_file(bin_path, d)
                self.assertEqual(len(results), 5)

                # Check first telemetry
                r = results[0]
                self.assertEqual(r["type"], "FW_PACKET_TELEM")
                self.assertEqual(r["channelName"], "Ref.cmdCount")
                self.assertEqual(r["value"], 5)
                self.assertEqual(r["time"]["seconds"], 1000)

                # Check enum telemetry
                r = results[2]
                self.assertEqual(r["channelName"], "Ref.heaterState")
                self.assertEqual(r["value"], "ON")

                # Check event with args
                r = results[3]
                self.assertEqual(r["type"], "FW_PACKET_LOG")
                self.assertEqual(r["eventName"], "Ref.CmdDispatched")
                self.assertEqual(r["args"][0]["value"], 0x1234)

                # Check event without args
                r = results[4]
                self.assertEqual(r["eventName"], "Ref.SystemReady")
                self.assertEqual(r["args"], [])
            finally:
                os.unlink(bin_path)
        finally:
            os.unlink(dict_path)

    def test_struct_channel_in_file(self):
        """Test decoding a telemetry channel whose type is a struct."""
        type_defs = [
            {
                "qualifiedName": "Nav.Quaternion",
                "kind": "struct",
                "members": {
                    "w": {"type": {"kind": "float", "size": 64}, "index": 0},
                    "x": {"type": {"kind": "float", "size": 64}, "index": 1},
                    "y": {"type": {"kind": "float", "size": 64}, "index": 2},
                    "z": {"type": {"kind": "float", "size": 64}, "index": 3},
                },
            }
        ]
        channels = [
            {
                "id": 300,
                "name": "Nav.attitude",
                "type": {"kind": "qualifiedIdentifier", "name": "Nav.Quaternion"},
            }
        ]
        dict_data = make_minimal_dictionary(channels=channels, type_definitions=type_defs)
        dict_path = write_temp_dict(dict_data)
        try:
            d = Dictionary(dict_path)
            value_bytes = struct.pack(">dddd", 1.0, 0.0, 0.0, 0.0)
            entry = make_telemetry_entry(300, value_bytes)
            file_data = make_com_file_data([entry])
            bin_path = write_temp_bin(file_data)
            try:
                results = decode_com_file(bin_path, d)
                self.assertEqual(len(results), 1)
                val = results[0]["value"]
                self.assertAlmostEqual(val["w"], 1.0)
                self.assertAlmostEqual(val["x"], 0.0)
                self.assertAlmostEqual(val["y"], 0.0)
                self.assertAlmostEqual(val["z"], 0.0)
            finally:
                os.unlink(bin_path)
        finally:
            os.unlink(dict_path)

    def test_array_channel_in_file(self):
        """Test decoding a telemetry channel whose type is an array."""
        type_defs = [
            {
                "qualifiedName": "Sensors.Readings",
                "kind": "array",
                "size": 4,
                "elementType": {"kind": "integer", "size": 16, "signed": True},
            }
        ]
        channels = [
            {
                "id": 400,
                "name": "Sensors.latest",
                "type": {"kind": "qualifiedIdentifier", "name": "Sensors.Readings"},
            }
        ]
        dict_data = make_minimal_dictionary(channels=channels, type_definitions=type_defs)
        dict_path = write_temp_dict(dict_data)
        try:
            d = Dictionary(dict_path)
            value_bytes = struct.pack(">hhhh", 10, -20, 30, -40)
            entry = make_telemetry_entry(400, value_bytes)
            file_data = make_com_file_data([entry])
            bin_path = write_temp_bin(file_data)
            try:
                results = decode_com_file(bin_path, d)
                self.assertEqual(len(results), 1)
                val = results[0]["value"]
                self.assertEqual(val, [10, -20, 30, -40])
            finally:
                os.unlink(bin_path)
        finally:
            os.unlink(dict_path)


class TestConstants(unittest.TestCase):
    """Tests to verify constant values match the F Prime defaults."""

    def test_packet_descriptor_size(self):
        self.assertEqual(PACKET_DESCRIPTOR_SIZE, 2)

    def test_chan_id_size(self):
        self.assertEqual(CHAN_ID_SIZE, 4)

    def test_event_id_size(self):
        self.assertEqual(EVENT_ID_SIZE, 4)

    def test_time_size(self):
        self.assertEqual(TIME_SIZE, 11)  # 2 + 1 + 4 + 4

    def test_tlm_packetize_id_size(self):
        self.assertEqual(TLM_PACKETIZE_ID_SIZE, 2)

    def test_packet_type_values(self):
        self.assertEqual(FW_PACKET_COMMAND, 0x0000)
        self.assertEqual(FW_PACKET_TELEM, 0x0001)
        self.assertEqual(FW_PACKET_LOG, 0x0002)
        self.assertEqual(FW_PACKET_FILE, 0x0003)
        self.assertEqual(FW_PACKET_PACKETIZED_TLM, 0x0004)
        self.assertEqual(FW_PACKET_DP, 0x0005)

    def test_packet_type_names(self):
        self.assertEqual(PACKET_TYPE_NAMES[FW_PACKET_TELEM], "FW_PACKET_TELEM")
        self.assertEqual(PACKET_TYPE_NAMES[FW_PACKET_LOG], "FW_PACKET_LOG")
        self.assertEqual(PACKET_TYPE_NAMES[FW_PACKET_COMMAND], "FW_PACKET_COMMAND")
        self.assertEqual(PACKET_TYPE_NAMES[FW_PACKET_PACKETIZED_TLM], "FW_PACKET_PACKETIZED_TLM")


class TestTimeTagValues(unittest.TestCase):
    """Tests to verify time tag parsing with different time base values."""

    def test_tb_none(self):
        data = make_time_bytes(time_base=0)  # TB_NONE
        result, _ = parse_time(data, 0)
        self.assertEqual(result["timeBase"], 0)

    def test_tb_proc_time(self):
        data = make_time_bytes(time_base=1)  # TB_PROC_TIME
        result, _ = parse_time(data, 0)
        self.assertEqual(result["timeBase"], 1)

    def test_tb_workstation_time(self):
        data = make_time_bytes(time_base=2)  # TB_WORKSTATION_TIME
        result, _ = parse_time(data, 0)
        self.assertEqual(result["timeBase"], 2)

    def test_tb_sc_time(self):
        data = make_time_bytes(time_base=3)  # TB_SC_TIME
        result, _ = parse_time(data, 0)
        self.assertEqual(result["timeBase"], 3)

    def test_tb_dont_care(self):
        data = make_time_bytes(time_base=0xFFFF)  # TB_DONT_CARE
        result, _ = parse_time(data, 0)
        self.assertEqual(result["timeBase"], 0xFFFF)


class TestMainFunction(unittest.TestCase):
    """Tests for the main() CLI function."""

    def test_main_end_to_end(self):
        """Test the full CLI pipeline: bin file -> JSON output."""

        channels = [
            {"id": 1, "name": "testChan", "type": {"kind": "integer", "size": 32, "signed": False}}
        ]
        dict_data = make_minimal_dictionary(channels=channels)
        dict_path = write_temp_dict(dict_data)

        entry = make_telemetry_entry(1, struct.pack(">I", 99))
        file_data = make_com_file_data([entry])
        bin_path = write_temp_bin(file_data)

        fd, output_path = tempfile.mkstemp(suffix=".json")
        os.close(fd)

        try:
            script_path = Path(__file__).parent / "comlogger_decoder.py"
            result = subprocess.run(
                [
                    sys.executable,
                    str(script_path),
                    "--bin-file", bin_path,
                    "--dictionary", dict_path,
                    "--output", output_path,
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, f"stderr: {result.stderr}")
            self.assertIn("Decoded 1 entries", result.stdout)

            with open(output_path, "r") as f:
                output = json.load(f)

            self.assertEqual(output["entryCount"], 1)
            self.assertEqual(output["entries"][0]["channelName"], "testChan")
            self.assertEqual(output["entries"][0]["value"], 99)
        finally:
            os.unlink(dict_path)
            os.unlink(bin_path)
            if os.path.exists(output_path):
                os.unlink(output_path)

    def test_main_no_store_length(self):
        """Test the CLI with --no-store-length and --fixed-size."""

        channels = [
            {"id": 1, "name": "ch", "type": {"kind": "integer", "size": 32, "signed": False}}
        ]
        dict_data = make_minimal_dictionary(channels=channels)
        dict_path = write_temp_dict(dict_data)

        entry = make_telemetry_entry(1, struct.pack(">I", 55))
        fixed_size = len(entry)
        file_data = make_com_file_data([entry], store_buffer_length=False)
        bin_path = write_temp_bin(file_data)

        fd, output_path = tempfile.mkstemp(suffix=".json")
        os.close(fd)

        try:
            script_path = Path(__file__).parent / "comlogger_decoder.py"
            result = subprocess.run(
                [
                    sys.executable,
                    str(script_path),
                    "--bin-file", bin_path,
                    "--dictionary", dict_path,
                    "--output", output_path,
                    "--no-store-length",
                    "--fixed-size", str(fixed_size),
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, f"stderr: {result.stderr}")

            with open(output_path, "r") as f:
                output = json.load(f)

            self.assertEqual(output["entryCount"], 1)
            self.assertEqual(output["entries"][0]["value"], 55)
        finally:
            os.unlink(dict_path)
            os.unlink(bin_path)
            if os.path.exists(output_path):
                os.unlink(output_path)

    def test_main_missing_fixed_size_errors(self):
        """Test that --no-store-length without --fixed-size produces an error."""

        dict_data = make_minimal_dictionary()
        dict_path = write_temp_dict(dict_data)
        bin_path = write_temp_bin(b"\x00")

        try:
            script_path = Path(__file__).parent / "comlogger_decoder.py"
            result = subprocess.run(
                [
                    sys.executable,
                    str(script_path),
                    "--bin-file", bin_path,
                    "--dictionary", dict_path,
                    "--output", "/tmp/out.json",
                    "--no-store-length",
                ],
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("--fixed-size is required", result.stderr)
        finally:
            os.unlink(dict_path)
            os.unlink(bin_path)


if __name__ == "__main__":
    unittest.main()
