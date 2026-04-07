#!/usr/bin/env python3
"""
ComLogger Decoder Tool

Decodes binary .com files produced by Svc::ComLogger into human-readable JSON
using the project's FPP JSON dictionary.

Usage:
    python comlogger_decoder.py --bin-file <file.com> --dictionary <dict.json> --output <output.json> [--no-store-length] [--fixed-size <size>]
"""

import argparse
import json
import struct
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


# Default type sizes (matching default/config/FpConfig.fpp and ComCfg.fpp)
PACKET_DESCRIPTOR_SIZE = 2  # U16
CHAN_ID_SIZE = 4             # U32
EVENT_ID_SIZE = 4            # U32
TIME_SIZE = 11               # U16 timeBase + U8 timeContext + U32 seconds + U32 useconds
TLM_PACKETIZE_ID_SIZE = 2   # U16

# Packet descriptor values (from default/config/ComCfg.fpp Apid enum)
FW_PACKET_COMMAND = 0x0000
FW_PACKET_TELEM = 0x0001
FW_PACKET_LOG = 0x0002
FW_PACKET_FILE = 0x0003
FW_PACKET_PACKETIZED_TLM = 0x0004
FW_PACKET_DP = 0x0005

PACKET_TYPE_NAMES = {
    FW_PACKET_COMMAND: "FW_PACKET_COMMAND",
    FW_PACKET_TELEM: "FW_PACKET_TELEM",
    FW_PACKET_LOG: "FW_PACKET_LOG",
    FW_PACKET_FILE: "FW_PACKET_FILE",
    FW_PACKET_PACKETIZED_TLM: "FW_PACKET_PACKETIZED_TLM",
    FW_PACKET_DP: "FW_PACKET_DP",
}


class Dictionary:
    """Loads and provides lookup for the FPP JSON dictionary."""

    def __init__(self, dict_path: str):
        with open(dict_path, "r") as f:
            self._dict = json.load(f)

        # Build lookup maps
        self._channels_by_id: Dict[int, dict] = {}
        for ch in self._dict.get("telemetryChannels", []):
            self._channels_by_id[ch["id"]] = ch

        self._events_by_id: Dict[int, dict] = {}
        for ev in self._dict.get("events", []):
            self._events_by_id[ev["id"]] = ev

        self._types_by_name: Dict[str, dict] = {}
        for td in self._dict.get("typeDefinitions", []):
            name = td.get("qualifiedName", td.get("name", ""))
            self._types_by_name[name] = td

    def get_channel(self, channel_id: int) -> Optional[dict]:
        return self._channels_by_id.get(channel_id)

    def get_event(self, event_id: int) -> Optional[dict]:
        return self._events_by_id.get(event_id)

    def get_type_definition(self, type_name: str) -> Optional[dict]:
        return self._types_by_name.get(type_name)

    @property
    def raw(self):
        return self._dict


def parse_time(data: bytes, offset: int) -> Tuple[dict, int]:
    """Parse Fw::Time from binary data. Returns (time_dict, new_offset)."""
    time_base = struct.unpack_from(">H", data, offset)[0]  # U16
    offset += 2
    time_context = struct.unpack_from(">B", data, offset)[0]  # U8
    offset += 1
    seconds = struct.unpack_from(">I", data, offset)[0]  # U32
    offset += 4
    useconds = struct.unpack_from(">I", data, offset)[0]  # U32
    offset += 4
    return {
        "timeBase": time_base,
        "timeContext": time_context,
        "seconds": seconds,
        "useconds": useconds,
    }, offset


def deserialize_value(
    data: bytes, offset: int, type_desc: dict, dictionary: Dictionary
) -> Tuple[Any, int]:
    """
    Deserialize a value from binary data based on its type descriptor from the dictionary.
    Returns (value, new_offset).

    Handles primitive types (integer, float, bool, string) and qualifiedIdentifier types
    by looking them up in the dictionary's typeDefinitions.
    """
    kind = type_desc.get("kind", "")

    if kind == "integer":
        size_bits = type_desc.get("size", 32)
        signed = type_desc.get("signed", False)
        size_bytes = size_bits // 8
        fmt_map = {
            (1, False): ">B",
            (1, True): ">b",
            (2, False): ">H",
            (2, True): ">h",
            (4, False): ">I",
            (4, True): ">i",
            (8, False): ">Q",
            (8, True): ">q",
        }
        fmt = fmt_map.get((size_bytes, signed))
        if fmt is None:
            # Unknown integer size, return raw hex
            raw = data[offset : offset + size_bytes]
            return raw.hex(), offset + size_bytes
        val = struct.unpack_from(fmt, data, offset)[0]
        return val, offset + size_bytes

    elif kind == "float":
        size_bits = type_desc.get("size", 32)
        if size_bits == 32:
            val = struct.unpack_from(">f", data, offset)[0]
            return val, offset + 4
        elif size_bits == 64:
            val = struct.unpack_from(">d", data, offset)[0]
            return val, offset + 8

    elif kind == "bool":
        val = struct.unpack_from(">B", data, offset)[0]
        return bool(val), offset + 1

    elif kind == "string":
        # F Prime strings are serialized as: [FwSizeStoreType length][chars]
        # FwSizeStoreType is U16 by default
        str_len = struct.unpack_from(">H", data, offset)[0]
        offset += 2
        val = data[offset : offset + str_len].decode("utf-8", errors="replace")
        return val, offset + str_len

    elif kind == "qualifiedIdentifier":
        # Look up the actual type in typeDefinitions
        type_name = type_desc.get("name", "")
        resolved = dictionary.get_type_definition(type_name)
        if resolved is None:
            # Can't resolve, return remaining bytes as hex
            return data[offset:].hex(), len(data)

        resolved_kind = resolved.get("kind", "")
        if resolved_kind == "enum":
            # Enums serialize as their representationType
            rep_type = resolved.get(
                "representationType",
                {"kind": "integer", "size": 32, "signed": True},
            )
            val, offset = deserialize_value(data, offset, rep_type, dictionary)
            # Try to find the enum constant name
            for ec in resolved.get("enumeratedConstants", []):
                if ec.get("value") == val:
                    return ec["name"], offset
            return val, offset

        elif resolved_kind == "struct":
            result = {}
            for member in resolved.get("members", []):
                member_name = member["name"]
                member_type = member["type"]
                member_val, offset = deserialize_value(
                    data, offset, member_type, dictionary
                )
                result[member_name] = member_val
            return result, offset

        elif resolved_kind == "array":
            arr_size = resolved.get("size", 0)
            elem_type = resolved.get("elementType", {})
            result = []
            for _ in range(arr_size):
                elem_val, offset = deserialize_value(
                    data, offset, elem_type, dictionary
                )
                result.append(elem_val)
            return result, offset

        elif resolved_kind == "alias":
            # Type alias - resolve to the underlying type
            underlying = resolved.get("type", {})
            return deserialize_value(data, offset, underlying, dictionary)

    # Fallback: return hex of remaining data
    return data[offset:].hex(), len(data)


def decode_telemetry_packet(payload: bytes, dictionary: Dictionary) -> dict:
    """Decode a FW_PACKET_TELEM payload (after the descriptor has been read)."""
    offset = 0
    # Channel ID (U32)
    chan_id = struct.unpack_from(">I", payload, offset)[0]
    offset += CHAN_ID_SIZE
    # Time tag
    time_tag, offset = parse_time(payload, offset)

    result = {
        "type": "FW_PACKET_TELEM",
        "channelId": chan_id,
        "time": time_tag,
    }

    ch_def = dictionary.get_channel(chan_id)
    if ch_def:
        result["channelName"] = ch_def["name"]
        try:
            value, _ = deserialize_value(payload, offset, ch_def["type"], dictionary)
            result["value"] = value
        except Exception:
            result["value"] = payload[offset:].hex()
            result["decodeError"] = "Failed to deserialize value"
    else:
        result["channelName"] = f"UNKNOWN_CHANNEL_{chan_id}"
        result["value"] = payload[offset:].hex()

    return result


def decode_event_packet(payload: bytes, dictionary: Dictionary) -> dict:
    """Decode a FW_PACKET_LOG payload (after the descriptor has been read)."""
    offset = 0
    # Event ID (U32)
    event_id = struct.unpack_from(">I", payload, offset)[0]
    offset += EVENT_ID_SIZE
    # Time tag
    time_tag, offset = parse_time(payload, offset)

    result = {
        "type": "FW_PACKET_LOG",
        "eventId": event_id,
        "time": time_tag,
    }

    ev_def = dictionary.get_event(event_id)
    if ev_def:
        result["eventName"] = ev_def["name"]
        result["severity"] = ev_def.get("severity", "UNKNOWN")
        result["formatString"] = ev_def.get("format", "")

        # Decode formal parameters
        args = []
        for param in ev_def.get("formalParams", []):
            try:
                val, offset = deserialize_value(
                    payload, offset, param["type"], dictionary
                )
                args.append({"name": param["name"], "value": val})
            except Exception:
                args.append(
                    {
                        "name": param["name"],
                        "value": payload[offset:].hex(),
                        "decodeError": True,
                    }
                )
                break
        result["args"] = args
    else:
        result["eventName"] = f"UNKNOWN_EVENT_{event_id}"
        result["args"] = payload[offset:].hex()

    return result


def decode_packetized_tlm_packet(payload: bytes, dictionary: Dictionary) -> dict:
    """Decode a FW_PACKET_PACKETIZED_TLM payload."""
    offset = 0
    # Packet ID (U16 FwTlmPacketizeIdType)
    pkt_id = struct.unpack_from(">H", payload, offset)[0]
    offset += TLM_PACKETIZE_ID_SIZE
    # Time tag
    time_tag, offset = parse_time(payload, offset)

    result = {
        "type": "FW_PACKET_PACKETIZED_TLM",
        "packetId": pkt_id,
        "time": time_tag,
        "rawData": payload[offset:].hex(),
    }

    # Packetized telemetry requires the telemetryPacketSets from the dictionary
    # to know which channels are in which packet and their order/sizes.
    raw_dict = dictionary.raw
    for pkt_set in raw_dict.get("telemetryPacketSets", []):
        for member in pkt_set.get("members", []):
            if member.get("id") == pkt_id:
                result["packetName"] = member.get("name", "UNKNOWN")
                # Try to decode individual channel values
                channels = []
                for ch_name in member.get("members", []):
                    # Find channel definition by name
                    ch_def = None
                    for ch in raw_dict.get("telemetryChannels", []):
                        if ch["name"] == ch_name:
                            ch_def = ch
                            break
                    if ch_def:
                        try:
                            val, offset = deserialize_value(
                                payload, offset, ch_def["type"], dictionary
                            )
                            channels.append({"name": ch_name, "value": val})
                        except Exception:
                            channels.append({"name": ch_name, "decodeError": True})
                            break
                    else:
                        channels.append(
                            {
                                "name": ch_name,
                                "decodeError": "Channel not found in dictionary",
                            }
                        )
                        break
                result["channels"] = channels
                break

    return result


def decode_packet(payload: bytes, dictionary: Dictionary) -> dict:
    """Decode a single ComBuffer payload."""
    if len(payload) < PACKET_DESCRIPTOR_SIZE:
        return {"type": "UNKNOWN", "error": "Payload too short", "raw": payload.hex()}

    descriptor = struct.unpack_from(">H", payload, 0)[0]
    packet_data = payload[PACKET_DESCRIPTOR_SIZE:]  # data after descriptor

    if descriptor == FW_PACKET_TELEM:
        return decode_telemetry_packet(packet_data, dictionary)
    elif descriptor == FW_PACKET_LOG:
        return decode_event_packet(packet_data, dictionary)
    elif descriptor == FW_PACKET_PACKETIZED_TLM:
        return decode_packetized_tlm_packet(packet_data, dictionary)
    else:
        type_name = PACKET_TYPE_NAMES.get(
            descriptor, f"UNKNOWN_0x{descriptor:04X}"
        )
        return {
            "type": type_name,
            "descriptor": descriptor,
            "raw": packet_data.hex(),
        }


def decode_com_file(
    file_path: str,
    dictionary: Dictionary,
    store_buffer_length: bool = True,
    fixed_size: Optional[int] = None,
) -> List[dict]:
    """
    Decode a .com file produced by Svc::ComLogger.

    Args:
        file_path: Path to the .com binary file
        dictionary: Loaded Dictionary object
        store_buffer_length: If True, each entry is prefixed with a U16 length.
                           If False, entries are fixed-size (fixed_size must be provided).
        fixed_size: Size of each entry when store_buffer_length is False.

    Returns:
        List of decoded packet dictionaries.
    """
    with open(file_path, "rb") as f:
        data = f.read()

    entries = []
    offset = 0
    entry_index = 0

    while offset < len(data):
        if store_buffer_length:
            # Read U16 size prefix
            if offset + 2 > len(data):
                break
            entry_size = struct.unpack_from(">H", data, offset)[0]
            offset += 2
            if entry_size == 0:
                break
            if offset + entry_size > len(data):
                entries.append(
                    {
                        "index": entry_index,
                        "error": f"Entry size {entry_size} exceeds remaining data ({len(data) - offset} bytes)",
                        "raw": data[offset:].hex(),
                    }
                )
                break
            payload = data[offset : offset + entry_size]
            offset += entry_size
        else:
            if fixed_size is None:
                raise ValueError(
                    "fixed_size must be provided when store_buffer_length is False"
                )
            if offset + fixed_size > len(data):
                break
            payload = data[offset : offset + fixed_size]
            offset += fixed_size

        decoded = decode_packet(payload, dictionary)
        decoded["index"] = entry_index
        entries.append(decoded)
        entry_index += 1

    return entries


def main():
    parser = argparse.ArgumentParser(
        description="Decode ComLogger .com binary files into human-readable JSON"
    )
    parser.add_argument(
        "--bin-file", required=True, help="Path to the .com binary file"
    )
    parser.add_argument(
        "--dictionary", required=True, help="Path to the FPP JSON dictionary"
    )
    parser.add_argument(
        "--output", required=True, help="Path for the output JSON file"
    )
    parser.add_argument(
        "--no-store-length",
        action="store_true",
        help="Set if ComLogger was configured with storeBufferLength=false",
    )
    parser.add_argument(
        "--fixed-size",
        type=int,
        default=None,
        help="Fixed entry size (required when --no-store-length is set)",
    )

    args = parser.parse_args()

    if args.no_store_length and args.fixed_size is None:
        parser.error("--fixed-size is required when --no-store-length is set")

    dictionary = Dictionary(args.dictionary)
    entries = decode_com_file(
        args.bin_file,
        dictionary,
        store_buffer_length=not args.no_store_length,
        fixed_size=args.fixed_size,
    )

    output = {
        "source": str(Path(args.bin_file).name),
        "dictionary": str(Path(args.dictionary).name),
        "entryCount": len(entries),
        "entries": entries,
    }

    with open(args.output, "w") as f:
        json.dump(output, f, indent=2)

    print(f"Decoded {len(entries)} entries from {args.bin_file} -> {args.output}")


if __name__ == "__main__":
    main()
