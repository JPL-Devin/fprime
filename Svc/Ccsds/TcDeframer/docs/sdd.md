# Svc::Ccsds::TcDeframer

The `Svc::Ccsds::TcDeframer` is an implementation of the [DeframerInterface](../../../Interfaces/docs/sdd.md) for the CCSDS [TC Space Data Link Protocol](https://ccsds.org/Pubs/232x0b4e1c1.pdf). 

It receives payload data (such as a Space Packet or a VCA_SDU) on input and produces a TC frame on its output port as a result. Please refer to the CCSDS [TC specification (CCSDS 232.0-B-4)](https://ccsds.org/Pubs/232x0b4e1c1.pdf) for details on the frame format and protocol.

The `Svc::Ccsds::TcDeframer` is designed to work in the common F Prime telemetry stack, receiving data from a [Communications Adapter](../../../Interfaces/docs/sdd.md) or the `Svc::FrameAccumulator`, for deframing and transmission to the rest of the system. It is commonly coupled with the [`Svc::Ccsds::SpacePacketDeframer`](../../SpacePacketFramer/docs/sdd.md) to unwrap CCSDS Space Packets from TC frames.

The TcDeframer currently functions only in the "Expedited Service" mode, for Type-B Frames. This means that should Type-A frames be received, no FARM checks would be performed on board.

## Configuration

The `TcDeframer` component can be configured with a specific Virtual Channel ID (VCID) and Spacecraft ID. By default, it uses the spacecraft ID from `config/ComCfg.fpp` and accepts all VCIDs.

```cpp
void configure(U16 vcId, U16 spacecraftId, bool acceptAllVcid);
```

- `vcId`: The virtual channel ID to accept. This is only used if `acceptAllVcid` is `false`.
- `spacecraftId`: The spacecraft ID to accept.
- `acceptAllVcid`: If `true`, the deframer accepts all VCIDs. If `false`, it only accepts the `vcId` specified.

Segment Header mode (see [Segment Header mode](#segment-header-mode)) is off by default and is enabled with:

```cpp
void configureSegmentHeader(bool enabled);
```

- `enabled`: If `true`, every accepted frame is expected to carry a one-octet CCSDS TC Segment Header right after the primary header. The octet is stripped from the emitted data and recorded, unmodified, in the emitted `ComCfg.FrameContext` (`tcSegmentHeaderPresent = true`, `tcSegmentHeader = <octet>`). If `false` (default), the deframer behaves exactly as it did before Segment Header support existed: no octet is stripped and the two context fields keep their defaults (`false`, `0`).

The last call wins; calling `configureSegmentHeader(true)` several times has the same effect as calling it once.

## Port Descriptions

| Kind | Name | Port Type | Description |
|---|---|---|---|
| Input (guarded) | dataIn | Svc.ComDataWithContext | Port to receive framed data |
| Output | dataOut | Svc.ComDataWithContext | Port to output deframed data. The emitted `ComCfg.FrameContext` carries the frame's Virtual Channel ID in its `vcId` field. |
| Output | dataReturnOut | Svc.ComDataWithContext | Port for returning ownership of received buffers to deframe |
| Input (sync) | dataReturnIn | Svc.ComDataWithContext | Port receiving back ownership of sent buffers |
| Output | errorNotify | Ccsds.ErrorNotify | Port to send notification of deframing errors |

## Events

| Name | Severity | Description |
|---|---|---|
| InvalidSpacecraftId | `warning low` | Deframing received an invalid SCID |
| InvalidFrameLength | `warning high` | Deframing received an invalid frame length |
| InvalidVcId | `activity low` | Deframing received an invalid VCID |
| InvalidCrc | `warning high` | Deframing received an invalid checksum |
| MissingSegmentHeader | `warning high` | Segment Header mode is on and a frame of the reported length has no octet after the primary header; the frame is dropped |
| ControlFrameDropped | `warning low` | Segment Header mode is on and a Type-BC/Type-AC control frame was received (Control Command Flag set); the frame is dropped |

## Segment Header mode

[CCSDS 232.0-B-4](https://ccsds.org/Pubs/232x0b4e1c1.pdf) §4.1.3.2.2 defines an optional one-octet Segment Header at the start of the Frame Data Field of a Type-BD frame. It carries the Sequence Flags (two bits) and the MAP ID (six bits) used to split a Space Packet across several frames and to multiplex several MAP channels on one Virtual Channel:

```
 octet:  0        1        2        3        4        5        6 ...          N-2  N-1
       +--------+--------+--------+--------+--------+--------+---------------+---------+
       |   TC Primary Header (5 octets)             |Segment | Segment data  |  FECF   |
       |                                            |Header  | (Space Packet |  (CRC)  |
       |                                            |        |  or a part)   |         |
       +--------+--------+--------+--------+--------+--------+---------------+---------+
                                                     ^
                                                     | 7 6 | 5 4 3 2 1 0 |
                                                     | seq |   MAP ID    |
                                                     |flags|             |
```

With Segment Header mode enabled, once a frame has passed all the existing checks (Spacecraft ID, frame length, Virtual Channel ID, CRC), the deframer:

1. Drops any frame whose Control Command Flag is set (Type-BC / Type-AC). Control frames carry no Segment Header (232.0-B-4 §4.1.3.2.1.4, §4.1.3.3), so octet 5 is never interpreted as one: the frame is returned upstream with a `ControlFrameDropped` event and no `errorNotify` (this is not a corrupted frame). With Segment Header mode off, control frames are forwarded like any other frame (baseline behaviour).
2. Drops any frame shorter than 5 + 1 + 2 octets (no room for a Segment Header between the primary header and the FECF) with a `MissingSegmentHeader` event and `errorNotify(TC_MISSING_SEGMENT_HEADER)`. Without this check the first FECF octet would be read as the Segment Header. A frame of exactly 5 + 1 + 2 octets is valid and is forwarded with an empty data field; rejecting empty segments is left to the downstream consumer.
3. Strips the Segment Header octet from the emitted data and copies it verbatim, without decoding or validating its Sequence Flags or MAP ID, into `ComCfg.FrameContext.tcSegmentHeader`, with `tcSegmentHeaderPresent` set to `true`. Downstream components (SDLS authentication, MAP reassembly) consume the octet from the context.

The deframer itself remains stateless. Segment Header mode is a deployment-wide switch for the Virtual Channels a `TcDeframer` instance serves: a frame stream mixing frames with and without Segment Headers on the same instance is not supported by the protocol and is not detected.

## Requirements

| Name | Description | Validation |
|---|---|---|
| SVC-CCSDS-TC-DEFRAMER-001 | The TcDeframer shall deframe Telecommand (TC) Transfer Frames according to the CCSDS Space Data Link Protocol standard for Type-BD frames. | Unit Test, Inspection |
| SVC-CCSDS-TC-DEFRAMER-002 | The TcDeframer shall perform Frame Validation Check Procedures, including Spacecraft ID, Virtual Channel ID, Frame Length, and CRC. | Unit Test |
| SVC-CCSDS-TC-DEFRAMER-003 | The TcDeframer shall be configurable for a specific Spacecraft ID. | Unit Test, Inspection |
| SVC-CCSDS-TC-DEFRAMER-004 | The TcDeframer shall be configurable with a specific Virtual Channel ID (VCID) OR to accept all VCIDs. | Unit Test, Inspection |
| SVC-CCSDS-TC-DEFRAMER-005 | The TcDeframer shall log an `InvalidSpacecraftId` event if a frame with an unexpected Spacecraft ID is received. | Unit Test |
| SVC-CCSDS-TC-DEFRAMER-006 | The TcDeframer shall log an `InvalidFrameLength` event if a frame with an invalid length is received. | Unit Test |
| SVC-CCSDS-TC-DEFRAMER-007 | The TcDeframer shall log an `InvalidVcId` event if a frame with an unexpected VCID is received (when not configured to accept all VCIDs). | Unit Test |
| SVC-CCSDS-TC-DEFRAMER-008 | The TcDeframer shall log an `InvalidCrc` event if a frame fails the CRC check. | Unit Test |
| SVC-CCSDS-TC-DEFRAMER-009 | The TcDeframer shall provide an input port (`dataIn`) to receive framed data, and emit deframed data packets on its `dataOut` output port. | Unit Test |
| SVC-CCSDS-TC-DEFRAMER-010 | The TcDeframer shall emit notifications on its `errorNotify` port when deframing errors occur. | Unit Test |
| SVC-CCSDS-TC-DEFRAMER-011 | The TcDeframer shall record the received frame's Virtual Channel ID in the `vcId` field of the `ComCfg.FrameContext` emitted on `dataOut`. | Unit Test |
| TCDEFRAMER-SH-001 | The TcDeframer shall be configurable, through `configureSegmentHeader(bool)`, to strip a one-octet CCSDS TC Segment Header from the Frame Data Field of every accepted Type-BD frame and to record the received octet unmodified in the `tcSegmentHeader` field of the emitted `ComCfg.FrameContext`, with `tcSegmentHeaderPresent` set to `true`. Segment Header mode shall be off by default. | Unit Test, Inspection |
| TCDEFRAMER-SH-002 | With Segment Header mode off, the TcDeframer shall emit exactly the same data and context as it did before Segment Header support existed, with `tcSegmentHeaderPresent = false` and `tcSegmentHeader = 0`. | Unit Test |
| TCDEFRAMER-SH-003 | With Segment Header mode on, the TcDeframer shall drop, return upstream, log a `MissingSegmentHeader` event and notify `TC_MISSING_SEGMENT_HEADER` on `errorNotify` for any frame shorter than the primary header, one Segment Header octet and the FECF (8 octets). This check shall run after the existing Spacecraft ID, frame length, Virtual Channel ID and CRC checks. | Unit Test |
| TCDEFRAMER-SH-004 | With Segment Header mode on, the TcDeframer shall drop, return upstream and log a `ControlFrameDropped` event for any frame with the Control Command Flag set (Type-BC / Type-AC) without interpreting its Frame Data Field. With Segment Header mode off such frames shall be forwarded unchanged. | Unit Test |
