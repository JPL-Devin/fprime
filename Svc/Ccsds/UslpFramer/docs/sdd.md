# Svc::Ccsds::UslpFramer

The `Svc::Ccsds::UslpFramer` is an implementation of the [FramerInterface](../../../Interfaces/docs/sdd.md) for the CCSDS [Unified Space Data Link Protocol (USLP)](https://ccsds.org/Pubs/732x1b3.pdf).

It receives payload data (such as a Space Packet) on input and produces a fixed-length USLP Transfer Frame on its output port as a result. Please refer to the CCSDS [USLP specification (CCSDS 732.1-B-3)](https://ccsds.org/Pubs/732x1b3.pdf) for details on the frame format and protocol.

The `Svc::Ccsds::UslpFramer` is designed to work in the common F Prime telemetry stack, receiving data from an upstream [`Svc::ComQueue`](../../../ComQueue/docs/sdd.md) and passing frames to a [Communications Adapter](../../../Interfaces/docs/sdd.md), such as a Radio manager component or [`Svc::ComStub`](../../../ComStub/docs/sdd.md), for transmission on the wire. It is commonly coupled with the [`Svc::Ccsds::SpacePacketFramer`](../../SpacePacketFramer/docs/sdd.md) to wrap CCSDS Space Packets into USLP frames.

## Scope

This implementation supports a deliberately constrained subset of the USLP standard, appropriate for the F Prime downlink stack:

- **Non-truncated frames only** (End of Frame Primary Header flag always 0, per 4.1.2.6).
- **No Operational Control Field** (OCF flag always 0, per 4.1.2.11.2).
- **No Insert Zone** (fixed frames without an Insert Service).
- **No segmentation and no packet spanning**: each frame carries exactly one complete payload unit aligned to the start of the TFDZ (First Header Pointer always 0). TFDZ construction rule 000 (fixed-length TFDZ, per table 4-3) is used with UPID 0b00000 (Space/Encapsulation Packets).
- **Expedited service only** (Bypass/Sequence Control flag = 1, per 4.1.2.8): no COP/FARM sequence control.
- **No OID (Only Idle Data) frame generation**: framing is on-demand — a frame is only produced when payload data arrives on `dataIn`, so there is never a need to generate a standalone idle frame.

## Frame Layout

Each output frame is exactly `ComCfg::UslpFrameFixedSize` octets:

```
+---------------------+-------------+---------------+----------------------------------+--------+
| Primary Header      | VCF Count   | TFDF Header   | TFDZ                             | FECF   |
| 7 octets (4.1.2)    | 4 octets    | 3 octets      | payload + Encapsulation Idle Pkt | 2 oct  |
|                     | (4.1.2.12)  | (4.1.4.2)     | fill (4.1.4.3.4)                 | (4.1.6)|
+---------------------+-------------+---------------+----------------------------------+--------+
```

The TFDF header is 3 octets: 1 octet of construction rules/UPID plus the 2-octet First Header Pointer (present for construction rules 000/001/010).

## Internals

The USLP protocol as used here specifies a fixed frame size. This can be configured in the `config/ComCfg.fpp` file (`ComCfg::UslpFrameFixedSize`).

The `Svc::Ccsds::UslpFramer` uses an internal (member) buffer to hold the fixed size frame. The buffer **must** be returned to the UslpFramer via the `dataReturnIn` port once it has been used or consumed. When the buffer returns to the UslpFramer it will reuse the buffer for the next frame. Should a component want to use the frame data past the time it is returned to the UslpFramer, data should be copied before the original buffer is returned to the UslpFramer via the `dataReturnIn` port.

### Idle Fill

Per CCSDS 732.1-B-3 paragraph 4.1.4.3.4, when the payload does not fill the fixed-length TFDZ, the remainder is completed with an **Encapsulation Idle Packet** (CCSDS 133.1-B-3 section 4.1.3.2):

| Gap (octets) | Fill |
|---|---|
| 0 | No fill needed |
| 1 | Single-octet idle packet (length-of-length 0b00) |
| 2–255 | Idle packet with 1-octet Packet Length field (length-of-length 0b01), total packet length = gap |
| ≥ 256 | Idle packet with Protocol ID Extension octet and 2-octet Packet Length field (length-of-length 0b10), total packet length = gap |

## CCSDS Header Fields

For each frame generated, the `Svc::Ccsds::UslpFramer` will populate the USLP Transfer Frame Primary Header fields as follows:

| Field | Value | Notes |
|---|---|---|
| Transfer Frame Version Number | 0b1100 | As per protocol 4.1.2.2 |
| Spacecraft ID | `ComCfg::SpacecraftId` | Set in the project configuration |
| Source-or-Destination Identifier | 0 (source) | Downlink frames originate at the spacecraft (4.1.2.3.4) |
| Virtual Channel ID | Set via `configure()` | 6 bits (4.1.2.4) |
| MAP ID | Set via `configure()` | 4 bits (4.1.2.5) |
| End of Frame Primary Header Flag | 0 | Non-truncated frames only (4.1.2.6) |
| Frame Length | `ComCfg::UslpFrameFixedSize - 1` | Total frame octets minus 1 (4.1.2.7) |
| Bypass/Sequence Control Flag | 1 (Expedited) | No COP sequence control (4.1.2.8) |
| Protocol Control Command Flag | 0 | User data (4.1.2.9) |
| Spares | 00 | (4.1.2.10) |
| OCF Flag | 0 | Unsupported (4.1.2.11.2) |
| VCF Count Length | 4 | 4-octet VCF Count field (4.1.2.11.4) |
| VCF Count | Incremented for each frame | Managed internally. One UslpFramer instance keeps count for a single Virtual Channel. |

## Port Descriptions

| Kind | Name | Port Type | Description |
|---|---|---|---|
| sync input | dataIn | Svc.ComDataWithContext | Receives data to frame, in a Fw::Buffer with optional context |
| output | dataOut | Svc.ComDataWithContext | Outputs framed data with optional context |
| output | dataReturnOut | Svc.ComDataWithContext | Returns ownership of the incoming Fw::Buffer to its sender once framing is handled |
| sync input | dataReturnIn | Svc.ComDataWithContext | Receives buffer from a deallocate call in a ComDriver component |
| sync input | comStatusIn | Fw.SuccessCondition | Receives status from downstream communication adapter per the [Communication Adapter Protocol](../../../../docs/reference/communication-adapter-interface.md#communication-adapter-protocol) |
| output | comStatusOut | Fw.SuccessCondition | Passes status through to upstream `Svc::ComQueue` per the [Framer Status Protocol](../../../../docs/reference/communication-adapter-interface.md#framer-status-protocol) |

## Requirements

| Name | Description | Validation |
|---|---|---|
| SVC-Ccsds-USLP-FRAMER-001 | The UslpFramer shall implement the `Svc.FramerInterface`. | Inspection, Unit Test |
| SVC-Ccsds-USLP-FRAMER-002 | The UslpFramer shall construct CCSDS USLP Transfer Frames compliant with the non-truncated frame format of the CCSDS 732.1-B-3 standard. | Unit Test, Inspection |
| SVC-Ccsds-USLP-FRAMER-003 | The UslpFramer shall use a fixed frame size that is configurable by the project. | Unit Test, Inspection |
| SVC-Ccsds-USLP-FRAMER-004 | The UslpFramer shall accept payload data to be framed via its `dataIn` port and output the constructed frame via its `dataOut` port. | Unit Test |
| SVC-Ccsds-USLP-FRAMER-005 | The UslpFramer shall return ownership of the input buffer via the `dataReturnOut` port after the framing process is complete. | Unit Test |
| SVC-Ccsds-USLP-FRAMER-006 | The UslpFramer shall accept returned frame buffers (previously sent via `dataOut`) through the `dataReturnIn` port for reuse. | Unit Test |
| SVC-Ccsds-USLP-FRAMER-007 | The UslpFramer shall receive communication status from downstream components via the `comStatusIn` port and pass it through to `comStatusOut`, per the [Framer Status Protocol](../../../../docs/reference/communication-adapter-interface.md#framer-status-protocol). | Unit Test, Integration Test |
| SVC-Ccsds-USLP-FRAMER-008 | The UslpFramer shall be configurable with a Virtual Channel ID (6 bits) and MAP ID (4 bits). | Inspection, Unit Test |
| SVC-Ccsds-USLP-FRAMER-009 | The UslpFramer shall correctly populate all mandatory fields of the USLP Transfer Frame Primary Header. | Unit Test, Inspection |
| SVC-Ccsds-USLP-FRAMER-010 | The UslpFramer shall maintain a 4-octet Virtual Channel Frame Count, incremented for each frame. | Unit Test |
| SVC-Ccsds-USLP-FRAMER-011 | The UslpFramer shall complete the fixed-length Transfer Frame Data Zone with an Encapsulation Idle Packet per CCSDS 732.1-B-3 4.1.4.3.4 and CCSDS 133.1-B-3 4.1.3.2. | Unit Test |
| SVC-Ccsds-USLP-FRAMER-012 | The UslpFramer shall compute and append a Frame Error Control Field (CRC-16) over the entire frame, per CCSDS 732.1-B-3 4.1.6. | Unit Test |
