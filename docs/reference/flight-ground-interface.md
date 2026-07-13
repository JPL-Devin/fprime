# Flight-Ground Interface: F´ Packet Structure

This document describes the on-the-wire structure of the data exchanged between F´ flight
software (FSW) and the Ground Data System (GDS). It is organized as a layered stack: the
outermost transport/framing layer is described first, followed by the layers it encapsulates,
down to the individual F´ payload packets (commands, events, telemetry, files).

Each layer wraps the layer below it. Reading top-to-bottom follows a byte as it arrives from the
link; reading bottom-to-top follows a payload as it is built for transmission.

> [!NOTE]
> All field sizes and layouts below reflect the **default** F´ configuration
> (`default/config/FpConfig.fpp`, `default/config/ComCfg.fpp`, `default/config/FpConstants.fpp`). Projects that override
> these configuration values change the corresponding field widths. Where a size depends on a
> configurable logical type, the type name is given alongside its default size.

## Conventions

### Endianness

Unless a component explicitly requests otherwise, F´ serializes all multi-byte integer fields in
**big-endian (network) byte order** — most significant byte first. This applies to every field in
this document: framing headers/trailers, CCSDS headers, the packet descriptor, command opcodes,
event/channel IDs, time tags, file-packet integers, and CRC/FECF values.

Source: `Fw/Types/Serializable.hpp`, `Fw/Types/Serializable.cpp`.

### Configurable logical types

Several identifier and size fields use F´ *logical types* whose width is set by configuration. The
defaults used throughout this document:

| Logical type | Default definition | Default size (bytes) | Used for |
|---|---|---:|---|
| `FwPacketDescriptorType` | `U16` | 2 | Com packet descriptor / APID enum |
| `FwIdType` | `U32` | 4 | Base for opcode / event / channel IDs |
| `FwOpcodeType` | `FwIdType` | 4 | Command opcode |
| `FwEventIdType` | `FwIdType` | 4 | Event (EVR) ID |
| `FwChanIdType` | `FwIdType` | 4 | Telemetry channel ID |
| `FwTlmPacketizeIdType` | `U16` | 2 | Packetized-telemetry packet ID |
| `FwTimeBaseStoreType` | `U16` | 2 | Time tag: time base |
| `FwTimeContextStoreType` | `U8` | 1 | Time tag: time context |

Sources: `default/config/FpConfig.fpp`, `default/config/ComCfg.fpp`.

## Layer overview

An F´ payload packet (command, event, telemetry, file) is always wrapped by the F´ *Com packet
descriptor*. That descriptor+payload unit (the "communications buffer") is then carried by exactly
**one** of two mutually exclusive framing stacks:

```
Option A — CCSDS stack                    Option B — Native F´ framing
------------------------------            ----------------------------
[ TC / TM / AOS transfer frame ]          [ F´ frame header (0xDEADBEEF + len) ]
  [ CCSDS Space Packet ]                    [ Com packet descriptor ]
    [ Com packet descriptor ]                 [ payload: Cmd / Log / Tlm / File ]
      [ payload: Cmd / Log / Tlm / File ]   [ F´ frame trailer (CRC32) ]
```

A given deployment uses **either** the CCSDS stack **or** the native F´ framing, selected by which
framer/deframer components are wired into the communications topology. The inner layers (Com packet
descriptor and the payload packets) are identical in both cases.

The remainder of this document walks the layers from the outside in:

1. [Framing / transfer-frame layer](#1-framing-transfer-frame-layer) (Option A: CCSDS; Option B: native F´)
2. [CCSDS Space Packet layer](#2-ccsds-space-packet-layer) (Option A only)
3. [F´ Com packet descriptor layer](#3-f-com-packet-descriptor-layer) (both)
4. [Payload packets](#4-payload-packets): command, event/log, telemetry, packetized telemetry, file
5. [Shared time-tag format](#5-shared-time-tag-format)

---

## 1. Framing / transfer-frame layer

This is the outermost layer on the space link. A deployment uses one of the two options below.

### Option A — CCSDS transfer frames

The CCSDS stack uses different frame types per direction:

- **Uplink (ground → flight):** TC (Telecommand) Transfer Frame.
- **Downlink (flight → ground):** TM (Telemetry) Transfer Frame, or AOS Transfer Frame.

In all CCSDS cases the transfer frame carries one or more **CCSDS Space Packets** (see
[Layer 2](#2-ccsds-space-packet-layer)) in its data field.

Sources: `Svc/Ccsds/Types/Types.fpp`, `Svc/Ccsds/TcDeframer`, `Svc/Ccsds/TmFramer`,
`Svc/Ccsds/AosFramer`, `Svc/Ccsds/AosDeframer`.

#### 1A.1 TC Transfer Frame (uplink)

5-byte primary header, variable data field, optional 2-byte Frame Error Control Field (FECF /
CRC16). F´ uses TC Type-BD frames (no FARM sequence checks).

```text
       0               1
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  0-1  |V  |B|C|R  |       SCID        |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  2-3  |    VCID   |     Frame Len     |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  4    | Frame Sequence Number |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

Legend: `Ver` = frame version (2b), `B` = bypass flag (1b), `C` = control-command flag (1b),
`Rsv` = reserved (2b), `SCID` = spacecraft ID (10b), `VCID` = virtual-channel ID (6b),
`Frame Len` = total frame length minus 1 (10b).

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | `flagsAndScId` (see bit layout) |
| 2 | 2 | `vcIdAndLength` (see bit layout) |
| 4 | 1 | Frame sequence number |
| 5 | N | Data field (CCSDS Space Packet) |
| 5+N | 2 | FECF (CRC16, optional) |

Bytes 0–1 `flagsAndScId`:

| Bits | Width | Field |
|---:|---:|---|
| 15–14 | 2 | Frame Version Number |
| 13 | 1 | Bypass Flag |
| 12 | 1 | Control Command Flag |
| 11–10 | 2 | Reserved |
| 9–0 | 10 | Spacecraft ID |

Bytes 2–3 `vcIdAndLength`:

| Bits | Width | Field |
|---:|---:|---|
| 15–10 | 6 | Virtual Channel ID |
| 9–0 | 10 | Frame Length (total frame bytes − 1) |

The FECF (when present) is a CRC16 computed over the entire frame except the final 2 FECF bytes.
Maximum frame is 5-octet header + up to 1019-octet data field (incl. optional FECF).

Source: `Svc/Ccsds/TcDeframer/TcDeframer.cpp`, `Svc/Ccsds/Types/Types.fpp`.

#### 1A.2 TM Transfer Frame (downlink)

Fixed-size frame (default `ComCfg::TmFrameFixedSize` = **1024 bytes**). 6-byte primary header,
data field filled with Space Packet(s) plus idle padding, 2-byte FECF trailer.

```text
       0               1
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  0-1  |V  |        SCID       |VCID |O|
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  2    |    Master Frame Count   |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  3    |   Virtual Channel Count |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  4-5  |H|S|O|Seg| First Hdr Ptr       |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

Legend: `Ver` = frame version (2b), `SCID` = spacecraft ID (10b), `VCID` = virtual-channel ID
(3b), `O` = OCF flag in word 0 and packet-order flag in word 2, `H` = secondary-header flag
(1b), `S` = synchronization flag (1b), `Seg` = segment-length ID (2b), `First Header Pointer`
= first packet-header offset (11b).

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | `globalVcId` (see bit layout) |
| 2 | 1 | Master channel frame count |
| 3 | 1 | Virtual channel frame count |
| 4 | 2 | `dataFieldStatus` (see bit layout) |
| 6 | … | Data field: Space Packet(s) + idle Space Packet padding |
| 1022 | 2 | FECF (CRC16) |

Bytes 0–1 `globalVcId`:

| Bits | Width | Field |
|---:|---:|---|
| 15–14 | 2 | Frame Version |
| 13–4 | 10 | Spacecraft ID |
| 3–1 | 3 | Virtual Channel ID |
| 0 | 1 | Operational Control Field (OCF) flag |

Bytes 4–5 `dataFieldStatus`:

| Bits | Width | Field |
|---:|---:|---|
| 15 | 1 | Secondary header flag |
| 14 | 1 | Synchronization flag |
| 13 | 1 | Packet order flag |
| 12–11 | 2 | Segment length ID |
| 10–0 | 11 | First header pointer |

F´ currently uses a single virtual channel (master and VC frame counts advance together), sets the
first-header pointer to 0 (the data field starts with a complete Space Packet), and pads the
remaining data field with a CCSDS idle Space Packet (idle APID `0x07FF`). The FECF is a CRC16 over
the full fixed-size frame except its final 2 bytes.

Source: `Svc/Ccsds/TmFramer/TmFramer.cpp`, `Svc/Ccsds/Types/Types.fpp`, `config/ComCfg.fpp`.

#### 1A.3 AOS Transfer Frame (downlink alternative)

6-byte AOS primary header + 2-byte M_PDU header + M_PDU packet zone + optional 2-byte FECF.
Payload (packet zone) starts at byte 8.

```text
       0               1
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  0-1  |V  |   SCID (LSBs) |   VCID    |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

       0       1       2       3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  2-5  |               VC Frame Count                   |R|C|SC |Cycle |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

Legend: `Ver` = AOS frame version (2b), `SCID` = spacecraft ID (8b in the first word plus 2b
in the signaling word), `VCID` = virtual-channel ID (6b), `R` = replay flag (1b), `C` = VC
frame-count cycle-use flag (1b), `VC Frame Count` = 24b count, `VC Cycle` = 4b cycle count.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | `globalVcId` (see bit layout) |
| 2 | 4 | `frameCountAndSignaling` (see bit layout) |
| 6 | 2 | M_PDU `firstHeaderPointer` |
| 8 | … | M_PDU packet zone (Space Packet(s)) |
| end−2 | 2 | FECF (CRC16, optional) |

Bytes 0–1 `globalVcId`:

| Bits | Width | Field |
|---:|---:|---|
| 15–14 | 2 | Frame Version |
| 13–6 | 8 | Spacecraft ID (LSBs) |
| 5–0 | 6 | Virtual Channel ID |

Bytes 2–5 `frameCountAndSignaling`:

| Bits | Width | Field |
|---:|---:|---|
| 31–8 | 24 | Virtual Channel Frame Count |
| 7 | 1 | Replay flag |
| 6 | 1 | VC frame-count usage (cycle) flag |
| 5–4 | 2 | Spacecraft ID (MSBs) |
| 3–0 | 4 | VC frame-count cycle |

The M_PDU `firstHeaderPointer` points to the first new CCSDS packet in the packet zone. Special
values: `0xFFFF` = no packet starts in this frame; `0xFFFE` = idle data only. The optional FECF is
a CRC16 over the entire frame except the final 2 FECF bytes.

Source: `Svc/Ccsds/AosFramer`, `Svc/Ccsds/Types/Types.fpp`.

### Option B — Native F´ framing (`FprimeFraming`)

The native F´ framing protocol wraps the communications buffer with an 8-byte header and a 4-byte
trailer. It does **not** use CCSDS Space Packets — the F´ Com packet descriptor
([Layer 3](#3-f-com-packet-descriptor-layer)) sits directly in the payload.

| Offset | Size | Field | Value |
|---:|---:|---|---|
| 0 | 4 | Start word (`TokenType` = `U32`) | `0xDEADBEEF` |
| 4 | 4 | Length field (`U32`) | payload length in bytes |
| 8 | N | Payload (Com packet descriptor + payload packet) | |
| 8+N | 4 | Trailer `crcField` (`U32`) | hash of header+payload |

```text
+====================+====================+================================+============+
| Start word (4)     | Length field (4)  | Payload (N)                    | Hash (4)    |
| 0xDEADBEEF         | payload bytes     | Com descriptor + packet        | crcField    |
+====================+====================+================================+============+
 0                    4                    8                            8+N       12+N
```

- The **start word** `0xDEADBEEF` is the frame delimiter/sync marker (there is no separate byte
  sync sequence).
- The **length field** is the size of the unframed payload only (excludes header and trailer).
- The **trailer** is computed by the configurable `Utils::Hash` backend over the header + payload
  (everything except the trailer itself). With the default CRC32 backend this is a 4-byte CRC32
  digest serialized big-endian. Although the field is named `crcField`, the algorithm follows
  whichever `Utils::Hash` implementation is configured.

Sources: `Svc/FprimeProtocol/FprimeProtocol.fpp`, `Svc/FprimeFramer/FprimeFramer.cpp`,
`Svc/FprimeDeframer/FprimeDeframer.cpp`, `Utils/Hash/Crc32/Crc32.hpp`.

---

## 2. CCSDS Space Packet layer

> Applies to **Option A (CCSDS)** only. When native F´ framing (Option B) is used, skip this layer —
> the Com packet descriptor sits directly in the frame payload.

Inside a CCSDS transfer frame's data field sits one or more **CCSDS Space Packets**. Each Space
Packet has a 6-byte primary header followed by its data field; F´ places the
[Com packet descriptor](#3-f-com-packet-descriptor-layer) and payload packet in that data field.
There is no Space Packet trailer/CRC at this layer.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | Packet Identification (see bit layout) |
| 2 | 2 | Packet Sequence Control (see bit layout) |
| 4 | 2 | Packet Data Length |
| 6 | N | Data field (F´ Com packet descriptor + payload packet) |

```text
       0               1
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  0-1  | Ver  |T|S|         APID         |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  2-3  |Seq |          Seq Count          |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  4-5  |       Packet Data Length       |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  6    | Data field: Com descriptor... |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

Legend: `Ver` = packet version number (3b), `T` = packet type (1b), `S` = secondary-header flag
(1b), `APID` = application process ID (11b), `Seq Flags` = sequence flags (2b), `Seq Count` =
packet sequence count (14b), `Packet Data Length` = data-field bytes minus 1 (16b).

Bytes 0–1 Packet Identification:

| Bits | Width | Field | F´ value |
|---:|---:|---|---|
| 15–13 | 3 | Packet Version Number | 0 |
| 12 | 1 | Packet Type | 0 (telemetry) / 1 (telecommand) |
| 11 | 1 | Secondary Header Flag | from frame context |
| 10–0 | 11 | APID | from frame context |

Bytes 2–3 Packet Sequence Control:

| Bits | Width | Field | F´ value |
|---:|---:|---|---|
| 15–14 | 2 | Sequence Flags | `0b11` (unsegmented) |
| 13–0 | 14 | Packet Sequence Count | per-APID counter |

Byte 4–5 **Packet Data Length**: per CCSDS, this is *(number of data-field bytes − 1)*. F´ writes
`dataLength = dataFieldSize - 1` and the deframer reconstructs `dataFieldSize = dataLength + 1`.
The data field (starting at byte 6) is the F´ communications buffer.

Sources: `Svc/Ccsds/SpacePacketFramer/SpacePacketFramer.cpp`,
`Svc/Ccsds/SpacePacketDeframer/SpacePacketDeframer.cpp`, `Svc/Ccsds/Types/Types.fpp`.

---

## 3. F´ Com packet descriptor layer

This layer is common to **both** framing options. Every F´ communications buffer begins with a
**packet descriptor** (`FwPacketDescriptorType`, default `U16`, 2 bytes, big-endian) that
identifies the type of payload packet that follows. `Fw::ComPacket` serializes this descriptor as
its first field.

```text
+====================+===============================================+
| Descriptor (2)     | Type-specific payload (0..510)                |
| FwPacketDescriptor | Command / Log / Tlm / File / other packet     |
+====================+===============================================+
 0                    2                                           512
```

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | Packet descriptor (`FwPacketDescriptorType`) |
| 2 | … | Payload packet (type selected by descriptor) |

Descriptor / APID enumeration values (`default/config/ComCfg.fpp`):

| Value | Name | Payload |
|---:|---|---|
| `0x0000` | `FW_PACKET_COMMAND` | [Command packet](#41-command-packet) |
| `0x0001` | `FW_PACKET_TELEM` | [Telemetry (channel) packet](#43-telemetry-channel-packet) |
| `0x0002` | `FW_PACKET_LOG` | [Event / log packet](#42-event-log-packet) |
| `0x0003` | `FW_PACKET_FILE` | [File packet](#45-file-packets) |
| `0x0004` | `FW_PACKET_PACKETIZED_TLM` | [Packetized telemetry packet](#44-packetized-telemetry-packet) |
| `0x0005` | `FW_PACKET_DP` | Data product packet |
| `0x0006` | `FW_PACKET_IDLE` | Idle packet |
| `0x00FE` | `FW_PACKET_HAND` | Handshake packet |
| `0x00FF` | `FW_PACKET_UNKNOWN` | Unknown/unset |
| `0x07FF` | `SPP_IDLE_PACKET` | CCSDS idle Space Packet APID |
| `0x0800` | `INVALID_UNINITIALIZED` | Sentinel (uninitialized) |

Sources: `Fw/Com/ComPacket.hpp`, `Fw/Com/ComPacket.cpp`, `default/config/ComCfg.fpp`.

---

## 4. Payload packets

Each payload packet below is the "innermost" content — it follows the 2-byte Com packet descriptor
from [Layer 3](#3-f-com-packet-descriptor-layer). Offsets in the tables are relative to the start
of the communications buffer (i.e. offset 0 is the descriptor), so the payload-specific fields
begin at offset 2.

The default communications buffer size is `FW_COM_BUFFER_MAX_SIZE` = **512 bytes**
(`default/config/FpConstants.fpp`), which bounds each payload's argument/value area.

### 4.1 Command packet

Descriptor `FW_PACKET_COMMAND` (`0x0000`). Carries an opcode plus a raw argument buffer. Uplink
only.

```text
+====================+====================+==============================+
| Descriptor (2)     | Opcode (4)         | Command args (0..506)        |
| = 0x0000           | FwOpcodeType       | serialized, no length prefix |
+====================+====================+==============================+
 0                    2                    6                         512
```

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | Descriptor = `FW_PACKET_COMMAND` |
| 2 | 4 | Opcode (`FwOpcodeType`) |
| 6 | remaining | Serialized command arguments (no length prefix) |

The argument bytes fill the rest of the buffer with no additional length field; argument decoding
is driven by the command dictionary. Default maximum argument area
`FW_CMD_ARG_BUFFER_MAX_SIZE` = 512 − `sizeof(FwOpcodeType)` − `sizeof(FwPacketDescriptorType)` =
**506 bytes**.

Sources: `Fw/Cmd/CmdPacket.hpp`, `Fw/Cmd/CmdPacket.cpp`, `Fw/Cmd/CmdArgBuffer.hpp`,
`default/config/FpConstants.fpp`.

### 4.2 Event / log packet

Descriptor `FW_PACKET_LOG` (`0x0002`). Carries an event (EVR) ID, a time tag, and serialized event
arguments. Downlink only.

```text
+====================+====================+=================+==============================+
| Descriptor (2)     | Event ID (4)       | Time tag (11)   | Event args (0..493)          |
| = 0x0002           | FwEventIdType      | Fw::Time        | serialized, no length prefix |
+====================+====================+=================+==============================+
 0                    2                    6                17                         512
```

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | Descriptor = `FW_PACKET_LOG` |
| 2 | 4 | Event ID (`FwEventIdType`) |
| 6 | 11 | Time tag (`Fw::Time`, see [§5](#5-shared-time-tag-format)) |
| 17 | remaining | Serialized event arguments (no length prefix) |

Default maximum argument area `FW_LOG_BUFFER_MAX_SIZE` = 512 − `sizeof(FwEventIdType)` −
`sizeof(FwPacketDescriptorType)` = **506 bytes** (before subtracting the time tag).

Sources: `Fw/Log/LogPacket.hpp`, `Fw/Log/LogPacket.cpp`, `Fw/Log/LogBuffer.hpp`.

### 4.3 Telemetry (channel) packet

Descriptor `FW_PACKET_TELEM` (`0x0001`). Carries one or more channel entries, each with a channel
ID, time tag, and serialized value. Downlink only.

```text
+====================+=================+=================+==============================+
| Descriptor (2)     | Channel ID (4)  | Time tag (11)   | Value (N)                    |
| = 0x0001           | FwChanIdType    | Fw::Time        | no length prefix             |
+====================+=================+=================+==============================+
 0                    2                 6                17
                                                         +==============================+
                                                         | ... next entry repeats       |
                                                         +==============================+
```

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | Descriptor = `FW_PACKET_TELEM` |
| 2 | 4 | Channel ID (`FwChanIdType`) |
| 6 | 11 | Time tag (`Fw::Time`, see [§5](#5-shared-time-tag-format)) |
| 17 | N | Serialized channel value (no length prefix) |
| 17+N | … | Next channel entry (ID + time tag + value), if present |

Each entry is `sizeof(FwChanIdType)` + 11 (time tag) + value bytes. Default maximum value area
`FW_TLM_BUFFER_MAX_SIZE` = 512 − `sizeof(FwChanIdType)` − `sizeof(FwPacketDescriptorType)` =
**506 bytes**.

Sources: `Fw/Tlm/TlmPacket.hpp`, `Fw/Tlm/TlmPacket.cpp`, `Fw/Tlm/TlmBuffer.hpp`.

### 4.4 Packetized telemetry packet

Descriptor `FW_PACKET_PACKETIZED_TLM` (`0x0004`), produced by `Svc::TlmPacketizer`. Instead of
per-channel IDs, a single packet ID selects a predefined packet layout; channel values are
concatenated at configured offsets.

```text
+====================+=================+=================+==============================+
| Descriptor (2)     | Packet ID (2)  | Time tag (11)   | Channel values (N)            |
| = 0x0004           | FwTlmPacketize  | Fw::Time        | configured offsets/sizes     |
+====================+=================+=================+==============================+
 0                    2                 4                15
```

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | Descriptor = `FW_PACKET_PACKETIZED_TLM` |
| 2 | 2 | Packet ID (`FwTlmPacketizeIdType`) |
| 4 | 11 | Packet time tag (`Fw::Time`, see [§5](#5-shared-time-tag-format)) |
| 15 | … | Concatenated serialized channel values |

There is **no** per-channel ID or per-channel length in the payload. The packet ID references a
packet definition (from configuration) that specifies each channel's offset and serialized size.

Sources: `Svc/TlmPacketizer/TlmPacketizer.cpp`, `Svc/TlmPacketizer/TlmPacketizerTypes.hpp`,
`default/config/FpConfig.fpp`.

### 4.5 File packets

Descriptor `FW_PACKET_FILE` (`0x0003`). Used for file uplink/downlink. The payload is an
`Fw::FilePacket`, which begins with its own 5-byte common header (a subtype byte + sequence index)
followed by subtype-specific fields.

```text
+====================+==============================+
| Type (1)           | Sequence index (4)           |
+====================+==============================+
 0                    1                              5
```

Common file-packet header (offsets relative to the start of the `Fw::FilePacket`, i.e. after the
2-byte Com descriptor):

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | Type (subtype) |
| 1 | 4 | Sequence index |

Subtype values: `T_START = 0`, `T_DATA = 1`, `T_END = 2`, `T_CANCEL = 3`, `T_NONE = 255`.

Path names (used in START packets) are encoded as a 1-byte length followed by that many path
characters (max 255).

#### START packet (`T_START`)

```text
+=======+==============+===========+====================+========================+
|Type(1)| Seq (4)      | File size | Src path (1+L)     | Dst path (1+L)         |
|START  |              | (4)       | len + bytes        | len + bytes            |
+=======+==============+===========+====================+========================+
 0       1              5           9                    10+sourceLen
```

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | Type = `T_START` |
| 1 | 4 | Sequence index (initialized to 0) |
| 5 | 4 | File size |
| 9 | 1 | Source path length |
| 10 | var | Source path bytes |
| … | 1 | Destination path length |
| … | var | Destination path bytes |

#### DATA packet (`T_DATA`)

```text
+=======+==============+==============+=============+=============================+
|Type(1)| Seq (4)      | File offset  | Data size   | File data (N)               |
|DATA   |              | (4)          | (2)         |                             |
+=======+==============+==============+=============+=============================+
 0       1              5              9             11
```

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | Type = `T_DATA` |
| 1 | 4 | Sequence index |
| 5 | 4 | File byte offset |
| 9 | 2 | Data size (`U16`) |
| 11 | N | File data bytes |

#### END packet (`T_END`)

```text
+=======+==============+==========================+
|Type(1)| Seq (4)      | File checksum (4)        |
|END    |              | CFDP checksum value      |
+=======+==============+==========================+
 0       1              5                          9
```

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | Type = `T_END` |
| 1 | 4 | Sequence index |
| 5 | 4 | File checksum (`U32`, from CFDP checksum) |

Total 9 bytes.

#### CANCEL packet (`T_CANCEL`)

```text
+=======+==============+
|Type(1)| Seq (4)      |
|CANCEL |              |
+=======+==============+
 0       1              5
```

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | Type = `T_CANCEL` |
| 1 | 4 | Sequence index |

Total 5 bytes (no fields after the common header).

Sources: `Fw/FilePacket/FilePacket.hpp`, `Fw/FilePacket/*.cpp`.

---

## 5. Shared time-tag format

Events, telemetry channels, and packetized telemetry all embed an `Fw::Time` time tag. With the
default configuration it serializes to **11 bytes**:

```text
+==================+===================+====================+====================+
| Time base (2)    | Time context (1) | Seconds (4)        | Microseconds (4)    |
| U16              | U8                | U32                | U32                |
+==================+===================+====================+====================+
 0                  2                   3                    7                  11
```

| Offset (within tag) | Size | Field | Type |
|---:|---:|---|---|
| 0 | 2 | Time base | `FwTimeBaseStoreType` (`U16`) |
| 2 | 1 | Time context | `FwTimeContextStoreType` (`U8`) |
| 3 | 4 | Seconds | `U32` |
| 7 | 4 | Microseconds | `U32` |

Time base enum values: `TB_NONE = 0`, `TB_PROC_TIME = 1`, `TB_WORKSTATION_TIME = 2`,
`TB_SC_TIME = 3`, `TB_DONT_CARE = 0xFFFF`.

All fields are big-endian. `Fw::Time::SERIALIZED_SIZE` =
`sizeof(FwTimeBaseStoreType)` + `sizeof(FwTimeContextStoreType)` + `sizeof(U32)` + `sizeof(U32)`.

Sources: `Fw/Time/Time.hpp`, `Fw/Time/Time.fpp`, `Fw/Time/Time.cpp`, `default/config/FpConfig.fpp`.
