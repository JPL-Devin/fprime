# Svc::Ccsds::UslpDeframer

The `Svc::Ccsds::UslpDeframer` is an implementation of the [DeframerInterface](../../../Interfaces/docs/sdd.md) for deframing uplink data that follows the [CCSDS 732.1-B-3 Unified Space Data Link Protocol (USLP)](https://ccsds.org/wp-content/uploads/gravity_forms/5-448e85c647331d9cbaf66c096458bdd5/2025/01//732x1b3e1.pdf) standard.

## Scope

Version 1 of this component supports a deliberately constrained USLP profile for uplink:

- Non-truncated transfer frames only (End of Frame Primary Header flag must be 0)
- Variable-length frames with the Frame Length field matching the received buffer size
- TFDF construction rule `0b111` (no segmentation, TFDZ contains a complete unit of data)
- Mandatory 2-octet Frame Error Control Field (FECF, CRC16)
- No Operational Control Field (OCF flag must be 0)
- No protocol control commands (Protocol Control Command flag must be 0)
- A configurable, fixed VCF Count field length (0 to 7 octets); the VCF count value itself is not checked in v1

## Configuration

`configure(vcId, spacecraftId, mapId, vcfCountLength, acceptAllVcid)` sets the acceptance parameters:

| Parameter | Default | Description |
|---|---|---|
| `vcId` | 0 | Virtual Channel ID to accept (6 bits) |
| `spacecraftId` | `ComCfg::SpacecraftId` | Spacecraft ID to accept (16 bits) |
| `mapId` | 0 | MAP ID to accept (4 bits) |
| `vcfCountLength` | 0 | Expected VCF Count field length in octets (0-7) |
| `acceptAllVcid` | `false` | Accept frames on any Virtual Channel |

Configuration values are trusted input and are validated with assertions at configuration time.

## Frame format

```
+---------------------------+-------------------+-------------+----------+--------+
| Primary Header (7 octets) | VCF Count (0-7)   | TFDF Header | TFDZ     | FECF   |
|                           | (per config)      | (1 octet)   | (var)    | (2)    |
+---------------------------+-------------------+-------------+----------+--------+
```

## Validation chain

Incoming frames are untrusted ground input: every check below rejects the frame with an event and
an `errorNotify` port invocation (if connected), and returns the buffer via `dataReturnOut`. The
component never asserts on frame content.

| # | Check | Error | Reference |
|---|---|---|---|
| 1 | Buffer larger than primary header + TFDF header + FECF (zero-payload frames rejected) | `USLP_INVALID_LENGTH` | 4.1.2 |
| 2 | Transfer Frame Version Number equals `0b1100` | `USLP_INVALID_VERSION` | 4.1.2.2.2 |
| 3 | End of Frame Primary Header flag is 0 (non-truncated) | `USLP_INVALID_HEADER` | 4.1.2.6 |
| 4 | Spacecraft ID matches configuration | `USLP_INVALID_SCID` | 4.1.2.3.2 |
| 5 | Source-or-Destination Identifier is 1 (spacecraft is destination on uplink) | `USLP_INVALID_HEADER` | 4.1.2.3.3 |
| 6 | Frame Length field + 1 equals received buffer size | `USLP_INVALID_LENGTH` | 4.1.2.7 |
| 7 | Virtual Channel ID matches configuration (unless `acceptAllVcid`) | `USLP_INVALID_VCID` | 4.1.2.4 |
| 8 | MAP ID matches configuration | `USLP_INVALID_MAP` | 4.1.2.5 |
| 9 | Protocol Control Command flag is 0 | `USLP_INVALID_HEADER` | 4.1.2.8.3 |
| 10 | Reserved spare bits are 0 | `USLP_INVALID_HEADER` | 4.1.2.8.4 |
| 11 | OCF flag is 0 (OCF unsupported in v1) | `USLP_INVALID_HEADER` | 4.1.2.8.5 |
| 12 | VCF Count Length matches configuration | `USLP_INVALID_HEADER` | 4.1.2.8.6 |
| 13 | Frame long enough for header + VCF count + TFDF header + FECF | `USLP_INVALID_LENGTH` | 4.1.2.7 |
| 14 | FECF CRC16 over frame minus trailer matches transmitted FECF | `USLP_INVALID_CRC` | 4.1.6 |
| 15 | TFDF construction rule is `0b111` (no segmentation) | `USLP_INVALID_TFDF` | 4.1.4.2.2 |

The UPID field of the TFDF header is reported in a diagnostic event but not strictly validated.

On success, the buffer is advanced past the primary header, VCF count and TFDF header, its size is
set to the TFDZ length (excluding the FECF), the frame context `vcId` is set to the received VCID,
and the buffer is emitted on `dataOut`.

## Port descriptions

| Kind | Name | Type | Description |
|---|---|---|---|
| `sync input` | dataIn | `Svc.ComDataWithContext` | Receives frames to deframe |
| `output` | dataOut | `Svc.ComDataWithContext` | Emits the deframed TFDZ payload |
| `sync input` | dataReturnIn | `Svc.ComDataWithContext` | Receives back ownership of buffers sent on dataOut |
| `output` | dataReturnOut | `Svc.ComDataWithContext` | Returns ownership of buffers received on dataIn |
| `output` | errorNotify | `Ccsds.ErrorNotify` | Notifies of deframing errors |

## Telemetry

| Name | Type | Description |
|---|---|---|
| FramesProcessed | U32 | Number of frames successfully deframed |
| CrcErrorCount | U32 | Number of FECF (CRC) errors |
