
# ComCcsds Subtopology

## References

- [ComCcsds Subtopology SDD](https://github.com/nasa/fprime/blob/devel/Svc/Subtopologies/ComCcsds/docs/sdd.md)
- [F Prime SpacePacketFramer SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/SpacePacketFramer/docs/sdd.md)
- [F Prime SpacePacketDeframer SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/SpacePacketDeframer/docs/sdd.md)
- [F Prime TmFramer SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/TmFramer/docs/sdd.md)
- [F Prime TcDeframer SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/TcDeframer/docs/sdd.md)
- [F Prime TcMapReassembler SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/TcMapReassembler/docs/sdd.md)
- [ComCcsdsSdls Subtopology SDD](https://github.com/nasa/fprime/blob/devel/Svc/Subtopologies/ComCcsdsSdls/docs/sdd.md)
- [F Prime ComQueue SDD](https://github.com/nasa/fprime/blob/devel/Svc/ComQueue/docs/sdd.md)

## Overview

The ComCcsds subtopology packages a CCSDS-standard communication stack as a reusable building block. It provides the complete data path for downlink (framing outgoing data into CCSDS Space Packets and TM Transfer Frames) and uplink (deframing incoming TC Transfer Frames and Space Packets and routing the contents to their destinations). This subtopology is appropriate for missions that require CCSDS-compliant space data link protocols.

Two variants are available, mirroring the ComFprime subtopology design:

1. **With ComStub** — Includes a ComStub adapter for connection to a byte stream driver.
2. **With External ComInterface** — The deployment provides its own communication adapter, typically used for custom radio implementations.

Each variant exists in two uplink forms: the **default** (`Subtopology`, `FramingSubtopology`; one Space Packet per TC frame, no Segment Header) and the **segmented** form (`SegmentedSubtopology`, `FramingSubtopologySegmented`) described under [Segmented uplink variants](#segmented-uplink-variants).

### Topology Diagram

The following diagram shows the complete ComCcsds subtopology:

![ComCcsds Subtopology](img/com-ccsds-topology.png)

### Downlink Path

![ComCcsds Downlink Path](img/com-ccsds-downlink.png)

Outgoing data follows a two-stage framing process:

1. The Communication Queue sends data to the Space Packet Framer, which constructs CCSDS Space Packets with proper APIDs and sequence counts.
2. The TM Framer wraps each Space Packet into a CCSDS TM Transfer Frame for transmission over the space link.

### Uplink Path

![ComCcsds Uplink Path](img/com-ccsds-uplink.png)

Incoming data follows a two-stage deframing process:

1. The Frame Accumulator collects bytes and assembles complete frames. The TC Deframer extracts Space Packets from TC Transfer Frames.
2. The Space Packet Deframer validates the Space Packets and extracts the payload, which is then routed by the F Prime Router to its destination (command dispatcher, file uplink, etc.).

### Segmented Uplink Variants

`ComCcsds.SegmentedSubtopology` (with ComStub) and `ComCcsds.FramingSubtopologySegmented` (external ComInterface) replace the transfer frame layer `TmTcFraming` with `TmTcFramingSegmented` and insert the `TcMapExtraction` layer between it and the packet layer:

```
Frame Accumulator -> TC Deframer (Segment Header mode, tcDeframerSeg) -> TC MAP Reassembler -> Space Packet Deframer -> F Prime Router
                                                                            ^  dedicated pool (tcPacketBufferManager)
```

- The TC Deframer instance `tcDeframerSeg` strips the 1-octet CCSDS TC **Segment Header** (Sequence Flags, MAP ID) into the frame context and drops Type-BC control frames.
- The **TC MAP Reassembler** demultiplexes the Frame Data Units by MAP ID (default: one MAP, ID 0) and reassembles Space Packets that span several TC frames; exactly one Space Packet per Frame Data Unit sequence (no blocking).
- `tcPacketBufferManager` is a **dedicated** `Svc.BufferManager` for reassembly, separate from the communications pool, sized by the `TcMapCfg` constants (see [Buffer Management](buffer-management.md)). It needs its own rate group connection (`tcPacketBufferManagerSchedIn` on `SegmentedSubtopology`, `TcMapExtraction.poolSchedIn` otherwise).

The segmented topologies are **additive**: they define three new instances (`tcDeframerSeg`, `tcPacketBufferManager`, `tcMapReassembler`) and reuse all the others, and the default topologies, the default `tcDeframer` and the generated code of a deployment importing the default topologies are unchanged. A deployment selects the uplink form by **which topology it imports** (exactly one of `Subtopology`/`FramingSubtopology` or `SegmentedSubtopology`/`FramingSubtopologySegmented`, since they share the packet layer instances); there is no runtime switch. The SDLS stack has the same two forms: `ComCcsdsSdls.SegmentedSubtopology`/`FramingSubtopologySegmented` place the SDLS decryption layer **between** the TC deframer and the MAP reassembler, so only authenticated Frame Data Units reach reassembly (requires a MAC-verifying decryptor such as `AesGcmDecryptor`; see the ComCcsdsSdls SDD). `TestDeploymentsProject/SubtopologyBuilds/Segmented` is a minimal deployment building both forms.

### Included Components

- **Space Packet Framer / Deframer** — CCSDS Space Packet Protocol layer
- **TM Framer** — CCSDS TM Transfer Frame construction for downlink
- **TC Deframer** — CCSDS TC Transfer Frame extraction for uplink
- **F Prime Router** — Routes deframed packets to their destinations
- **Communication Queue** — Queues and prioritizes outgoing data
- **Frame Accumulator** — Assembles complete frames from byte stream input
- **ComStub** (variant A only) — Byte stream driver adapter
- **TC MAP Reassembler** and dedicated **Buffer Manager** (segmented variants only) — CCSDS TC MAP packet extraction

### Configuration

- Base IDs, queue sizes, stack sizes, priorities, and CPU affinities via ComCcsdsConfig.
- Segmented variants: MAP channel count, maximum reassembled packet size and pool depth via the `TcMapCfg` constants (`Svc/Ccsds/TcMapReassembler/config`), MAP ID table via `ComCcsdsTcMapConfig.fpp` in ComCcsdsConfig.
- CCSDS-specific parameters (APIDs, virtual channels) are configured through the protocol components.

### Required Inputs

- A rate group connection to drive the Communication Queue.
- Segmented variants: a rate group connection to the dedicated reassembly Buffer Manager (`tcPacketBufferManagerSchedIn`).
- A byte stream driver (variant A) or custom ComInterface (variant B).
- Telemetry, event, and file downlink sources connected to the Communication Queue.
- Router outputs connected to the command dispatcher and file uplink.
