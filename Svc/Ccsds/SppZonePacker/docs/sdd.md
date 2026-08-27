# Svc::Ccsds::SppZonePacker

The `Svc::Ccsds::SppZonePacker` packs CCSDS Space Packets into fixed-size data zones, allowing multiple packets to share one transfer frame and a single packet to span consecutive frames of a virtual channel. It implements the [FramerInterface](../../../Interfaces/docs/sdd.md) and sits between the [`Svc::Ccsds::SpacePacketFramer`](../../SpacePacketFramer/docs/sdd.md) and a fixed-frame framer ([`Svc::Ccsds::TmFramer`](../../TmFramer/docs/sdd.md), [`Svc::Ccsds::AosFramer`](../../AosFramer/docs/sdd.md), or a fixed-length USLP framer), optionally with the [`Svc::Ccsds::CcsdsSdlsFramer`](../../CcsdsSdlsFramer/docs/sdd.md) in between:

```
ComQueue → SpacePacketFramer → SppZonePacker → [CcsdsSdlsFramer] → TmFramer/AosFramer → ComStub
```

Without a packer, one-packet-per-frame framing pads every frame with an Idle Packet, wasting downlink bandwidth. The packer eliminates that padding except when explicitly flushing a partial zone.

## First Header Pointer

Each emitted zone carries a canonical First Header Pointer (FHP) in `FrameContext.firstHeaderPointer`:

| Canonical value (`ComCfg.FhpValues`) | Meaning |
|---|---|
| Byte offset (`0 ..`) | Offset of the first Space Packet header that starts in this zone |
| `FHP_NO_PACKET_START` (0xFFFE) | The zone contains only continuation data of a spanning packet |
| `FHP_IDLE_DATA_ONLY` (0xFFFD) | The zone contains only idle data |
| `FHP_UNSET` (0xFFFF) | Default: no packer in the chain (legacy framer behavior) |

The downstream framer maps the canonical value to its protocol wire encoding (TM: 11-bit FHP in the Data Field Status, `0x7FF`/`0x7FE` sentinels per CCSDS 132.0-B-3 4.1.2.7.6; AOS: M_PDU FHP; fixed-length USLP: 16-bit TFDZ FHP for construction rules '000'). Variable-length USLP modes have no FHP and must not use this packer.

**Ground-side requirement**: the receiving system must perform FHP-aware reassembly of Space Packets across frames (as the [`Svc::Ccsds::AosDeframer`](../../AosDeframer/docs/sdd.md) does for AOS). One-packet-per-frame ground extraction will not work with a packer in the chain.

## Zero-copy framing

The packer assembles zones in place inside frame-sized member buffers: each zone buffer is a window into a `ComCfg.MaxTransferFrameSize`-bounded backing store, offset by a configured `headroom` (space for the downstream frame header and, when SDLS is used, the SA index) and followed by a configured `trailerReserve` (space for the frame trailer/FECF). Emitted contexts set `FrameContext.zeroCopyFrame = true`, letting downstream framers build the frame around the zone without copying. The buffer is owned by the packer and must flow back on `dataReturnIn` after transmission.

## Configuration

`configure(zoneSize, headroom, trailerReserve, vcId)` must be called once before use, with `headroom + zoneSize + trailerReserve <= ComCfg.MaxTransferFrameSize`. For TM: `zoneSize = TmFrameFixedSize - TMHeader size - TMTrailer size`, `headroom = TMHeader size` (+2 with SDLS), `trailerReserve = TMTrailer size`.

State is kept per virtual channel; the current implementation supports a single configured VCID, structured for later multi-VC expansion.

## Internals

- Packets are copied into the current zone; a packet that does not fit is split, with its remaining tail held (with an offset) as the single outstanding fragment until the in-flight zone buffer returns.
- Full zones are emitted immediately; partially filled zones are held so subsequent packets can be appended.
- A flush (`context.sendNow` or the `run` scheduler port) completes a partial zone with an Idle Packet. When fewer than 7 bytes (the minimum Space Packet size) remain, a minimum-size idle packet is striped across the zone boundary into the next zone.
- Flow control follows the [Framer Status Protocol](../../../../docs/reference/communication-adapter-interface.md#framer-status-protocol): the packer self-emits `SUCCESS` upstream when it consumes a packet without filling a zone (so `Svc::ComQueue` keeps sending), and absorbs one downstream `SUCCESS` per zone it emitted on its own credit (`creditOwed`, bounded at 2) to avoid double-crediting. `FAILURE` and initial/recovery `SUCCESS` pass through.
- The component is passive and protects its state with an internal mutex using a collect-then-emit pattern: output ports are never invoked while the lock is held, tolerating synchronous downstream chains (e.g. `Svc::ComStub`) re-entering the component.

## Port Descriptions

| Kind | Name | Port Type | Description |
|---|---|---|---|
| sync input | dataIn | Svc.ComDataWithContext | Receives Space Packets to pack, with context |
| output | dataOut | Svc.ComDataWithContext | Outputs packed data zones with FHP in context |
| output | dataReturnOut | Svc.ComDataWithContext | Returns ownership of consumed packet buffers upstream |
| sync input | dataReturnIn | Svc.ComDataWithContext | Receives zone buffers back from downstream |
| sync input | comStatusIn | Fw.SuccessCondition | Receives downstream communication status |
| output | comStatusOut | Fw.SuccessCondition | Passes/self-generates status to upstream `Svc::ComQueue` |
| sync input | run | Svc.Sched | Scheduler input driving periodic flush of partial zones |

## Requirements

| Name | Description | Validation |
|---|---|---|
| SVC-CCSDS-SPP-ZONE-PACKER-001 | The SppZonePacker shall implement the `Svc.FramerInterface`. | Inspection, Unit Test |
| SVC-CCSDS-SPP-ZONE-PACKER-002 | The SppZonePacker shall pack multiple Space Packets into a fixed-size data zone. | Unit Test |
| SVC-CCSDS-SPP-ZONE-PACKER-003 | The SppZonePacker shall split Space Packets across consecutive zones of a virtual channel. | Unit Test |
| SVC-CCSDS-SPP-ZONE-PACKER-004 | The SppZonePacker shall set `FrameContext.firstHeaderPointer` on each emitted zone to the offset of the first packet header starting in the zone, or to the applicable canonical sentinel. | Unit Test |
| SVC-CCSDS-SPP-ZONE-PACKER-005 | The SppZonePacker shall complete partial zones with CCSDS Idle Packets on flush (`sendNow` or scheduler), striping a minimum-size idle packet across the zone boundary when fewer than 7 bytes remain. | Unit Test |
| SVC-CCSDS-SPP-ZONE-PACKER-006 | The SppZonePacker shall preserve the Framer Status Protocol: pass through initial, failure, and recovery statuses; self-credit consumed packets; and absorb downstream statuses for zones it emitted on its own credit. | Unit Test |
| SVC-CCSDS-SPP-ZONE-PACKER-007 | The SppZonePacker shall emit zones as windows into member buffers with configured headroom and trailer reserve, marked `zeroCopyFrame` in the context, and shall not allocate memory after initialization. | Unit Test, Inspection |
| SVC-CCSDS-SPP-ZONE-PACKER-008 | The SppZonePacker shall hold at most one outstanding packet fragment per virtual channel and shall not queue packets internally. | Inspection |
| SVC-CCSDS-SPP-ZONE-PACKER-009 | The SppZonePacker shall tolerate synchronous re-entry from downstream components without deadlock. | Unit Test |

## Scope

This component addresses downlink only. TC uplink segmentation is out of scope. For simple whole-buffer concatenation without packet spanning, see [`Svc::ComAggregator`](../../../ComAggregator/docs/sdd.md); in CCSDS fixed-frame stacks the SppZonePacker supersedes it.
