
# CCSDS Protocol Functionality

## References

- [F Prime SpacePacketFramer SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/SpacePacketFramer/docs/sdd.md)
- [F Prime SpacePacketDeframer SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/SpacePacketDeframer/docs/sdd.md)
- [F Prime ApidManager SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/ApidManager/docs/sdd.md)
- [F Prime TmFramer SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/TmFramer/docs/sdd.md)
- [F Prime TcDeframer SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/TcDeframer/docs/sdd.md)
- [F Prime AosFramer SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/AosFramer/docs/sdd.md)
- [F Prime CcsdsSdlsFramer SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/CcsdsSdlsFramer/docs/sdd.md)
- [F Prime CcsdsSdlsDeframer SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/CcsdsSdlsDeframer/docs/sdd.md)
- [F Prime SdlsSaRouter SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/SdlsSaRouter/docs/sdd.md)
- [F Prime SdlsFileKeyManager SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/SdlsFileKeyManager/docs/sdd.md)
- [F Prime AesGcmEncryptor SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/AesGcmEncryptor/docs/sdd.md)
- [F Prime AesGcmDecryptor SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/AesGcmDecryptor/docs/sdd.md)
- [CCSDS Space Packet Protocol (133.0-B-2)](https://ccsds.org/Pubs/133x0b2e2.pdf)
- [CCSDS TM Space Data Link Protocol (132.0-B-3)](https://ccsds.org/Pubs/132x0b3.pdf)
- [CCSDS TC Space Data Link Protocol (232.0-B-4)](https://ccsds.org/Pubs/232x0b4e1c1.pdf)
- [CCSDS AOS Space Data Link Protocol (732.0-B-5)](https://ccsds.org/wp-content/uploads/gravity_forms/5-448e85c647331d9cbaf66c096458bdd5/2025/10/732x0b5ec1.pdf)

## Overview

F Prime provides components implementing the Consultative Committee for Space Data Systems (CCSDS) protocols for missions requiring standards-compliant space communication. These components plug into the communication stack in place of (or alongside) the default F Prime protocol components, providing CCSDS-standard framing at multiple protocol layers. The implementation covers the Space Packet Protocol, TM and TC Space Data Link protocols, and the AOS Space Data Link protocol.

### Space Packet Protocol

The Space Packet layer provides application-level packet framing per CCSDS 133.0-B-2:

- **Space Packet Framer** — Constructs CCSDS Space Packets from user data. Each packet is assigned an Application Process Identifier (APID) and a sequence count for tracking.
- **Space Packet Deframer** — Extracts and validates received Space Packets, verifying the packet structure and sequence counts to detect dropped or out-of-order packets.

### APID Management

The APID Manager tracks per-APID sequence counts for both outgoing and incoming Space Packets. It provides incrementing sequence counts to the Space Packet Framer for each APID and validates received sequence counts in the Space Packet Deframer to detect packet loss.

By default, APIDs are assigned based on the F Prime data descriptor type (commands, telemetry, events, files, packetized telemetry). Missions requiring custom APID assignments can replace the default APID Manager component with a project-specific implementation. To add project-specific data types with their own APIDs, see the [Add Custom Uplink and Downlink Data Types](../../how-to/develop/custom-uplink-downlink-data.md) guide.

### TM Space Data Link Protocol

The TM Framer implements the CCSDS Telemetry (TM) Space Data Link Protocol (132.0-B-3) for downlink. It wraps payload data (such as Space Packets) into TM Transfer Frames for transmission over the space link. The current implementation supports a single Virtual Channel Identifier (VCID). `ComCfg::AggregationSize` is the full TM data field available to the upstream [ComAggregator](https://github.com/nasa/fprime/blob/devel/Svc/ComAggregator/docs/sdd.md); with spanning disabled, its maximum aggregate is `ComCfg::AggregationSize - 7` so the framer can add an idle packet. When packet spanning is enabled in the upstream [ComAggregator](https://github.com/nasa/fprime/blob/devel/Svc/ComAggregator/docs/sdd.md) (`ComCcsdsConfig.Aggregator.enablePacketSpanning`), Space Packets may span consecutive TM Transfer Frames and the First Header Pointer in each frame locates the first packet header, per 132.0-B-3 section 4.1.2.7.6; spanning is disabled by default. Enabling it requires a ground deframer that reassembles spanned packets using the First Header Pointer.

### TC Space Data Link Protocol

The TC Deframer implements the CCSDS Telecommand (TC) Space Data Link Protocol (232.0-B-4) for uplink. It extracts payload data from received TC Transfer Frames.

By default the Transfer Frame Data Field is delivered as is (no Segment Header; one Space Packet per frame). The deframer also has a **Segment Header mode** (`TcDeframer::configureSegmentHeader(true)`, selected in the `ComCcsds` subtopology by the distinct `tcDeframerSeg` instance) in which the 1-octet Segment Header (232.0-B-4 section 4.1.3.2) following the primary header is validated and recorded in the `FrameContext` (`tcSegmentHeaderPresent`, `tcSegmentHeader`) and only the Frame Data Unit is forwarded. The Segment Header carries the **Sequence Flags** in its upper two bits (`01` FIRST, `00` CONTINUING, `10` LAST, `11` UNSEGMENTED) and the **MAP ID** in its lower six bits. Type-BC (control) frames are dropped with the `ControlFrameDropped` event in this mode, and a frame too short to carry the Segment Header is rejected with `MissingSegmentHeader`.

#### MAP packet extraction

In Segment Header mode the [TcMapReassembler](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/TcMapReassembler/docs/sdd.md) (232.0-B-4 section 6.5.3, MAP Packet Extraction) sits between the TC deframer and the Space Packet deframer. It demultiplexes frames by `(Virtual Channel, MAP ID)` pair onto a configured set of MAP channels (`TcMapReassembler::configure(channels, count)`; default one channel, VCID 1 / MAP ID 0; the same MAP ID on two Virtual Channels is two independent channels), and reassembles one Space Packet per channel from a FIRST/CONTINUING.../LAST sequence of Frame Data Units, or forwards an UNSEGMENTED frame directly. Reassembly buffers come from a **dedicated, bounded** buffer pool (`tcPacketBufferManager`, sized by the `TcMapCfg` constants — see [Buffer Management](buffer-management.md)); the completed packet is handed to the Space Packet deframer only when its length matches the Space Packet header length field. Exactly one Space Packet is carried per Frame Data Unit sequence (no **blocking** of several packets into one FDU), the reassembled packet is bounded by `TcMapCfg.MaxPacketSize`, and frames whose `(VCID, MAP ID)` pair is not configured are rejected. The segmented topologies (`ComCcsds.SegmentedSubtopology` and friends) are additive: the default `ComCcsds.Subtopology` and default `tcDeframer` are unchanged, see [ComCcsds subtopology](subtopology-com-ccsds.md).

### AOS Space Data Link Protocol

The AOS Framer implements the CCSDS Advanced Orbiting Systems (AOS) Space Data Link Protocol (732.0-B-5). AOS provides an alternative to TM for missions requiring more flexible virtual channel management.

### Space Data Link Security (SDLS)

An optional SDLS layer provides per-frame encryption and decryption keyed by a 16-bit security association (SA) index:

- **CcsdsSdlsFramer** — Delegates encryption of outgoing data and prepends the SA index to build the SDLS frame (downlink).
- **CcsdsSdlsDeframer** — Extracts the SA index from incoming frames and delegates decryption (uplink).
- **SdlsSaRouter** — Routes encryption/decryption requests to downstream crypto components based on the SA index.
- **SdlsFileKeyManager** — Supplies encryption keys read from a configured file.
- **ClearTextEncryptor / ClearTextDecryptor** — Pass-through default crypto components (**no security**); the defaults selected by the `Svc.ComCcsdsSdls` subtopology configuration.
- **AesGcmEncryptor / AesGcmDecryptor** — AES-256-GCM authenticated encryption (OpenSSL 3.x), producing/consuming an `IV (12) | ciphertext | MAC (16)` security payload with the VC and SA index authenticated as additional data. The decryptor reports a failed MAC check as `MAC_VERIFICATION_FAILURE`, distinct from `DECRYPTION_FAILURE`. Each frame's 28-byte overhead must be subtracted from `ComCfg.AggregationSize` (see the `Svc.ComCcsdsSdls` SDD). On the TC side the decryptor authenticates the received frame header (19 octets of AAD by default); when the frame carries a Segment Header (`FrameContext.tcSegmentHeaderPresent`) the **received** Segment Header octet is included in the AAD (20 octets), so a forged Sequence Flags/MAP ID fails the MAC check.

**Ordering with MAP packet extraction.** In the segmented SDLS topology (`ComCcsdsSdls.SegmentedSubtopology`) the processing order is the receiving-end order of 232.0-B-4 section 6.5.2.1: frame validation and Segment Header extraction (`tcDeframerSeg`), then SDLS processing (`sdlsDeframer` → SA router → decryptor), then MAP packet extraction (`tcMapReassembler`). The reassembler is connected to the SDLS deframer's output, which only carries frames the decryptor accepted (`SdlsStatus::SUCCESS`); no unauthenticated frame can allocate, append to, complete or abandon reassembly state. This property holds only with a MAC-verifying decryptor such as `AesGcmDecryptor`: the default `ClearTextDecryptor` accepts every frame (and reports `NullCipherInUse`).

### Protocol Layering

The CCSDS components can be stacked to provide multiple protocol layers. A typical downlink path might be: data source → Space Packet Framer → TM Framer → byte stream driver. A typical uplink path: byte stream driver → Frame Accumulator → TC Deframer → Space Packet Deframer → Router. The optional SDLS layer sits between the Space Packet layer and the transfer frame layer in both directions (see the `Svc.ComCcsdsSdls` subtopology). The modular design allows missions to select the specific protocol layers they require.

### Unsupported Features

The current CCSDS implementation does not support:

- Multiple Virtual Channel Identifiers (VCIDs) — only a single VCID is available per TM Framer or TC Deframer instance.
- TC **blocking** (several Space Packets in one Frame Data Unit): exactly one Space Packet per FDU sequence; the MAP reassembler rejects a completed FDU whose length does not match the Space Packet header.
- TC Segment Header Sequence Flags/MAP ID are honored only by the segmented topologies (`tcDeframerSeg` + `tcMapReassembler`); the default `tcDeframer` forwards the Transfer Frame Data Field without interpreting a Segment Header.
- TC MAP Access Service (MAP_SDU) and Virtual Channel Access/Frame services are not provided; only MAP Packet Service is.
- SDLS Extended Procedures (SA 0 is reserved and left unconnected).

### Off Nominal

- Malformed frames or packets are rejected with diagnostic events.
- Sequence count mismatches indicate dropped or reordered packets and are reported via events. The onboard sequence count is synchronized to the received value and processing continues.
- CRC or checksum failures cause the affected frame to be dropped.
- In Segment Header mode, a Type-BC control frame is dropped (`ControlFrameDropped`) and a frame too short for the Segment Header is rejected (`MissingSegmentHeader`).
- In the MAP reassembler, a frame for an unconfigured MAP ID, a CONTINUING/LAST without a preceding FIRST, a FIRST/UNSEGMENTED while a reassembly is in progress (the partial packet is abandoned), an accumulated length exceeding `TcMapCfg.MaxPacketSize` or a completed length that does not match the Space Packet header are each reported by an event and the affected Frame Data Unit(s) dropped; the reassembly buffer is returned to the dedicated pool. A pool exhaustion drops the frame without allocating.
- In the segmented SDLS topology an SDLS authentication failure on one segment is reported by the decryptor and the frame is dropped before reaching the reassembler; the partial packet in progress on that MAP keeps its buffer (there is no stall timeout) until the next authenticated FIRST/UNSEGMENTED, LAST or overflow on that MAP releases it.
