# Svc::Ccsds::TcMapReassembler

`Svc::Ccsds::TcMapReassembler` is an implementation of the [DeframerInterface](../../../Interfaces/docs/sdd.md) that reassembles CCSDS Space Packets from the Frame Data Units (FDUs) of TC Transfer Frames carrying a Segment Header ([CCSDS 232.0-B-4](https://ccsds.org/Pubs/232x0b4e1c1.pdf) 4.1.3.2.2, 4.4.1, 4.4.3). It sits on the uplink path after `Svc::Ccsds::TcDeframer` in Segment Header mode (and, when SDLS is used, after the `Svc::Ccsds::CcsdsSdlsDeframer` SUCCESS gate) and before `Svc::Ccsds::SpacePacketDeframer`.

Each incoming buffer is the *portion* of one FDU: the Space Packet bytes carried by one frame, with the Segment Header already stripped by `TcDeframer` into `ComCfg::FrameContext` (`tcSegmentHeaderPresent`, `tcSegmentHeader`). The reassembler decodes the sequence flags and MAP ID from the context, accumulates the portions of a packet per MAP, and delivers exactly one complete Space Packet per FDU on `dataOut`. Several MAPs may have a packet in progress at the same time (232.0-B-4 4.4.3.3); *blocking* (several packets in one FDU, 232.0-B-4 4.4.1, optional) is not supported and is rejected by the exact-length check.

## Requirements

| Name | Description | Validation |
|---|---|---|
| TCMAP-001 | The component shall reassemble one Space Packet from an UNSEGMENTED FDU or from a FIRST, zero or more CONTINUING, and a LAST segment on the same MAP, and deliver it on `dataOut`. | Unit test |
| TCMAP-002 | The component shall keep independent reassembly state for up to `TcMapCfg.MapChannelCount` configured MAP IDs, and shall reject segments for unconfigured MAP IDs without changing any state. | Unit test |
| TCMAP-003 | The component shall reject, without allocating or changing state, any input whose context has `tcSegmentHeaderPresent == false`. | Unit test |
| TCMAP-004 | The component shall bound every reassembled packet to `TcMapCfg.MaxPacketSize` octets, checking the portion, the declared Space Packet length and the accumulated length before any copy. | Unit test, STest |
| TCMAP-005 | The component shall deliver a packet only if the accumulated length equals the length declared in the Space Packet primary header (133.0-B-2 4.1.2.2); otherwise it shall discard the partial packet. | Unit test |
| TCMAP-006 | The component shall return every incoming frame buffer on `dataReturnOut` synchronously within `dataIn`, on every path. | Unit test, STest |
| TCMAP-007 | The component shall obtain reassembly buffers only from its `allocate` port, copy every portion into them, validate the allocation result, and release them through `deallocate` on abandon or on `dataReturnIn`. | Unit test, STest |
| TCMAP-008 | The component shall emit an event and a `Ccsds.FrameError` on `errorNotify` for every rejection or abandon, and shall count delivered, dropped and abandoned outcomes in telemetry. | Unit test, STest |
| TCMAP-009 | The component shall not allocate memory after initialization and shall not use a mutex other than the guarded `dataIn` port. | Inspection |

## Configuration

```cpp
struct MapKey {
    U8 vcId;   // TC Virtual Channel ID (0..63), matched against FrameContext.vcId
    U8 mapId;  // MAP ID (0..63) of the Segment Header
};
void configure(const MapKey* channels, FwSizeType count);
```

`configure` sets the table of accepted `(Virtual Channel, MAP ID)` pairs, one reassembly channel each. A MAP is a channel *within* one Virtual Channel (232.0-B-4 2.1.3), and `TcDeframer` accepts several VCIDs, so the reassembly state is keyed by the pair: the same MAP ID on two Virtual Channels is two independent channels that never share a partial packet. `configure` must be called once at initialization with `1 <= count <= TcMapCfg::MapChannelCount`, every `vcId` and `mapId` in `0..63`, distinct pairs, and a non-null table; violations are programming errors and assert. A segment whose `(FrameContext.vcId, MAP ID)` pair is not in the table is rejected (`InvalidMapId`). The default table of the `ComCcsds` segmented subtopologies is `{{1, 0}}` (VCID 1 is the fprime-gds TC framing default).

Each configured pair costs one `MapChannel` and one reassembly buffer of `MaxPacketSize` in the dedicated pool (`PoolBufferCount = MapChannelCount + MaxPacketsInFlight`), whether or not the pair is ever used.

Compile-time configuration lives in the `TcMapCfg` FPP module (`config/TcMapReassemblerConfig/TcMapCfg.fpp`), registered with `register_fprime_config`; a project overrides it by shadowing the file in its own configuration directory.

| Constant | Default | Meaning |
|---|---|---|
| `MapChannelCount` | 1 | Number of MAP slots (1..64) |
| `MaxPacketSize` | 4096 | Largest reassembled Space Packet, in octets (7..65542) |
| `MaxPacketsInFlight` | 4 | Delivered packets that may be outstanding downstream before allocation fails |
| `PoolBufferCount` | `MapChannelCount + MaxPacketsInFlight` | Buffers in the dedicated pool |
| `PoolBytes` | `PoolBufferCount * MaxPacketSize` | Pool memory |
| `PoolManagerId` | 201 | Manager ID of the dedicated `Svc.BufferManager` |
| `SpacePacketHeaderSize`, `SpacePacketMinSize`, `SpacePacketMaxSize` | 6, 7, 65542 | CCSDS 133.0-B-2 constants used by the static assertions |

`TcMapReassembler.hpp` includes the generated `TcMapReassemblerConfig/FppConstantsAc.hpp` and pins the arithmetic with `static_assert`s (`MapChannelCount` in 1..64, `MaxPacketSize` in 7..65542, `MaxPacketsInFlight >= 1`, `PoolBufferCount` and `PoolBytes` consistent, `PoolBufferCount` representable in `Svc::BufferManager::BufferBin::numBuffers`). Every compared FPP constant is first bound to a named `constexpr` because FPP constants are anonymous enums and the framework compiles with `-Wconversion -Werror`.

The pool must be a **dedicated** `Svc.BufferManager` instance with one bin of `PoolBufferCount` buffers of `MaxPacketSize` octets. `Svc::BufferManager` allocates first-fit over all bins, so sharing bins with the communications pool would let either side starve the other. Projects doing file uplink through a segmented MAP must set `MaxPacketsInFlight >= FileHandlingConfig.QueueSizes.fileUplink`.

## Port Descriptions

| Kind | Name | Port Type | Description |
|---|---|---|---|
| Input (guarded) | dataIn | Svc.ComDataWithContext | FDU portion with its Segment Header in the `ComCfg.FrameContext` |
| Output | dataOut | Svc.ComDataWithContext | Complete Space Packet (a pool buffer, `setSize` to the packet length) |
| Output | dataReturnOut | Svc.ComDataWithContext | Returns ownership of the incoming frame buffer, synchronously on every path |
| Input (sync) | dataReturnIn | Svc.ComDataWithContext | Receives back a delivered packet buffer; forwards it to `deallocate` |
| Output | errorNotify | Ccsds.ErrorNotify | `Ccsds.FrameError` for every rejection or abandon |
| Output | allocate | Fw.BufferGet | Requests a `MaxPacketSize` buffer from the dedicated pool |
| Output | deallocate | Fw.BufferSend | Returns a reassembly buffer to the dedicated pool |

## State Machine

One `MapChannel` per configured `(VCID, MAP ID)` pair: `state` (`IDLE` or `IN_PROGRESS`), `buffer` (valid iff `IN_PROGRESS`, capacity `MaxPacketSize`), `received` (octets copied so far) and `segments`. `Max` below is `TcMapCfg::MaxPacketSize`; "portion" is the size of the incoming buffer; "declared" is `6 + be16(data[4..5]) + 1`, or 0 when fewer than 6 octets are available.

Preconditions checked before the table, in order: `tcSegmentHeaderPresent == true` (else `SegmentHeaderAbsent`), `(FrameContext.vcId, MAP ID)` configured (else `InvalidMapId`), `portion > 0` (else `EmptySegment`). None of them changes state or allocates.

| Flags \ State | IDLE | IN_PROGRESS |
|---|---|---|
| **FIRST** | `portion > Max` → `PacketTooLarge` / `TC_SEGMENT_OVERFLOW` / drop, stay IDLE. Else if `portion >= 6` and declared `> Max` → `PacketTooLarge` / `TC_SEGMENT_OVERFLOW` / drop, stay IDLE (**before** allocation). Else allocate `Max`; failure → `AllocationFailed` / `TC_SEGMENT_ALLOC_FAILED` / drop, stay IDLE. Else copy → **IN_PROGRESS**. | Abandon partial: deallocate, `PacketAbandoned(map, received, FIRST)` / `TC_SEGMENT_UNEXPECTED_FIRST` / `PacketsAbandoned++`, IDLE; then process exactly as FIRST × IDLE. |
| **CONTINUING** | `UnexpectedSegment(map, CONTINUING)` / `TC_SEGMENT_ORPHAN` / `SegmentsDropped++`, stay IDLE. | `received + portion > Max` → `PacketTooLarge` / `TC_SEGMENT_OVERFLOW` / deallocate / IDLE / `PacketsAbandoned++`. Else append, stay IN_PROGRESS. |
| **LAST** | `UnexpectedSegment(map, LAST)` / `TC_SEGMENT_ORPHAN` / `SegmentsDropped++`, stay IDLE. | Overflow as CONTINUING. Else append, then **complete**: declared ≠ received → `LengthMismatch` / `TC_SEGMENT_LENGTH_MISMATCH` / deallocate / IDLE / `PacketsAbandoned++`; else `setSize(received)`, IDLE, `PacketsReassembled++`, `dataOut`. |
| **UNSEGMENTED** | As FIRST × IDLE (size checks before allocation, allocate, copy), then **complete** as LAST. Never leaves IDLE on exit. | Abandon partial (`PacketAbandoned(map, received, UNSEGMENTED)` / `TC_SEGMENT_UNEXPECTED_UNSEGMENTED` / `PacketsAbandoned++`, IDLE); then process as UNSEGMENTED × IDLE. |

The incoming frame is returned on `dataReturnOut` in every cell. The MAP is set to IDLE *before* `dataOut_out` is invoked.

### Every path

| # | Condition (checked in this order) | MAP state | Reassembly buffer | Event | `FrameError` | Telemetry |
|---|---|---|---|---|---|---|
| P1 | `tcSegmentHeaderPresent == false` | none (no MAP lookup) | none | `SegmentHeaderAbsent` (WARNING_HI) | `TC_SEGMENT_HEADER_ABSENT` | `SegmentsDropped++` |
| P2 | `(vcId, MAP ID)` pair not configured | none | none | `InvalidMapId` (WARNING_LO; vcId, mapId) | `TC_INVALID_MAP_ID` | `SegmentsDropped++` |
| P3 | `portion == 0` | none | none | `EmptySegment` (WARNING_LO; mapId, flags) | `TC_SEGMENT_EMPTY` | `SegmentsDropped++` |
| P4 | FIRST/UNSEGMENTED while IN_PROGRESS | IN_PROGRESS → IDLE, then continue as IDLE | deallocate partial | `PacketAbandoned` (WARNING_HI; mapId, received, flags) | `TC_SEGMENT_UNEXPECTED_FIRST` / `TC_SEGMENT_UNEXPECTED_UNSEGMENTED` | `PacketsAbandoned++` |
| P5 | CONTINUING/LAST while IDLE | none | none | `UnexpectedSegment` (WARNING_HI; mapId, flags) | `TC_SEGMENT_ORPHAN` | `SegmentsDropped++` |
| P6 | FIRST/UNSEGMENTED: `portion > Max` | none | none | `PacketTooLarge` (WARNING_HI; mapId, portion, Max) | `TC_SEGMENT_OVERFLOW` | `SegmentsDropped++` |
| P8 | FIRST/UNSEGMENTED: `portion >= 6` and declared `> Max`, evaluated before `allocate` | none | none (nothing allocated) | `PacketTooLarge` (WARNING_HI; mapId, declared, Max) | `TC_SEGMENT_OVERFLOW` | `SegmentsDropped++` |
| P7 | FIRST/UNSEGMENTED: `allocate` returns `!isValid()` or `getSize() < Max` | none | a valid short buffer is passed to `deallocate` exactly once | `AllocationFailed` (WARNING_HI; mapId, Max) | `TC_SEGMENT_ALLOC_FAILED` | `SegmentsDropped++` |
| P9 | CONTINUING/LAST: `received + portion > Max` | IN_PROGRESS → IDLE | deallocate partial | `PacketTooLarge` (WARNING_HI; mapId, received + portion, Max) | `TC_SEGMENT_OVERFLOW` | `PacketsAbandoned++` |
| P10 | LAST/UNSEGMENTED complete: `received < 6` or declared ≠ received | → IDLE | deallocate | `LengthMismatch` (WARNING_HI; mapId, received, declared or 0) | `TC_SEGMENT_LENGTH_MISMATCH` | `PacketsAbandoned++` |
| P11 | LAST/UNSEGMENTED complete: exact match | → IDLE before `dataOut` | `setSize(received)`; ownership passes downstream; returned on `dataReturnIn` → `deallocate` | none | none | `PacketsReassembled++` |
| P12 | FIRST accepted / CONTINUING appended | IDLE → IN_PROGRESS / stays | copy into pool buffer | none | none | none |

Events are throttled (`SegmentHeaderAbsent` after 5, the others after 10 occurrences); `errorNotify` and telemetry are not.

## Buffer Ownership

The reassembler is **copy-always**: the incoming frame buffer is never retained, not even for UNSEGMENTED packets. Every portion is copied into a buffer obtained from `allocate`, and the frame buffer is returned on `dataReturnOut` before `dataIn` returns. The upstream owner (`Svc::FrameAccumulator`, through the deframers) therefore never needs to know whether the frame was consumed, and pool accounting never depends on the downstream path.

| Buffer | Allocated by | Held by | Released by |
|---|---|---|---|
| Frame (uplink) | upstream (`Svc::FrameAccumulator`) | `TcMapReassembler` only for the duration of `dataIn` | `dataReturnOut`, synchronously, on every path |
| Reassembly buffer | `allocate` (dedicated `Svc.BufferManager`) at FIRST/UNSEGMENTED | `MapChannel.buffer` while IN_PROGRESS | `deallocate` on abandon (P4), overflow (P9), mismatch (P10) |
| Completed packet (same buffer, `setSize(received)`) | — | downstream (`SpacePacketDeframer` → router → sink) | `dataReturnIn` → `deallocate` |

Allocation results are validated: an invalid buffer or one shorter than `MaxPacketSize` is treated as an allocation failure (a valid short buffer is deallocated once, nothing is retained). Every `memcpy` is preceded by a size check against `MaxPacketSize`; no memory is allocated after initialization.

### Threading (no mutex)

`dataIn` is a guarded port and is the only writer of the MAP table, the reassembly buffers and the counters. `dataReturnIn` is synchronous and touches no MAP state: it only forwards the returned buffer to `deallocate` (`Svc::BufferManager::bufferSendIn` is itself guarded). Because a MAP is set to IDLE before `dataOut_out`, a downstream component returning the buffer synchronously on the same call stack re-enters only `dataReturnIn`, which neither reads MAP state nor takes the component mutex. `dataReturnIn` must not be made guarded: the component mutex is error-checking on POSIX and a synchronous re-entry would fail the lock.

## SDLS Ordering and Held Buffers

In the SDLS topology the reassembler is connected after the `CcsdsSdlsDeframer` SUCCESS gate, so no unauthenticated byte can allocate, append to, complete or abandon reassembly state. A frame whose MAC fails is returned upstream by the SDLS deframer and never reaches `dataIn`.

After an SDLS authentication failure on a middle segment, the reassembler receives nothing; the MAP keeps its authenticated prefix and one pool buffer until the next FIRST/UNSEGMENTED on that MAP, a LAST (length mismatch) or an overflow. There is no timeout. One pool buffer per MAP is therefore reserved for in-progress packets and `MaxPacketsInFlight` remain for delivered packets.

The partial is released by the next authenticated FIRST/UNSEGMENTED (P4), the next authenticated LAST (P10, unless a retransmitted segment completes the packet exactly, in which case it is delivered) or an accumulated overflow (P9). The pool is sized for this (`PoolBufferCount = MapChannelCount + MaxPacketsInFlight`) as a capacity argument; `Svc::BufferManager` does not reserve per MAP.

## Events

| Name | Severity | Throttle | Description |
|---|---|---|---|
| SegmentHeaderAbsent | `warning high` | 5 | Frame reached the reassembler without Segment Header context (`TcDeframer` Segment Header mode off) |
| InvalidMapId | `warning low` | 10 | `(Virtual Channel, MAP ID)` pair not in the configured set |
| UnexpectedSegment | `warning high` | 10 | CONTINUING or LAST while the MAP is IDLE |
| PacketAbandoned | `warning high` | 10 | Partial discarded because a FIRST or UNSEGMENTED arrived while IN_PROGRESS |
| EmptySegment | `warning low` | 10 | Segment with zero user-data octets |
| PacketTooLarge | `warning high` | 10 | Portion, declared length or accumulation exceeds `MaxPacketSize` |
| AllocationFailed | `warning high` | 10 | Dedicated pool returned an invalid or short buffer |
| LengthMismatch | `warning high` | 10 | Accumulated length differs from the declared Space Packet length |

## Telemetry

| Name | Type | Description |
|---|---|---|
| PacketsReassembled | U32 | Space Packets delivered on `dataOut` |
| SegmentsDropped | U32 | Inputs rejected without changing MAP state (P1, P2, P3, P5, P6, P7, P8) |
| PacketsAbandoned | U32 | Partial packets discarded (P4, P9, P10) |

## Ground Tooling Requirements

| Requirement | YAMCS | NASA CryptoLib | F´ GDS 4.3.0 |
|---|---|---|---|
| Segment Header on every Type-BD frame of the VC | yes when configured | `has_segmentation_hdr` | **no** |
| Flags `11` for one-frame packets | yes, only mode | pass-through | — |
| `01/00*/10` for spanning packets, no MAP interleaving within a MAP | **no**: rejects packets that do not fit one frame | pass-through | — |
| MAP ID in the configured set (default 0) | configurable | configurable | — |
| One Space Packet per FDU, no fill | yes | — | yes |
| Packet ≤ `MaxPacketSize` (4096 default) | trivially (one frame) | — | ≤ 1016 |
| Portion ≤ 1016 (no SDLS) / 986 (SDLS) | one-frame limit | — | 1016 |
| SDLS: segment first, ApplySecurity per frame, Security Header after the Segment Header, Segment Header in the AAD | yes | yes | no SDLS |

YAMCS and CryptoLib interoperate today for UNSEGMENTED (flags `11`) packets with SDLS; spanning packets need a ground framer that segments. `fprime-gds` does not emit Segment Headers, so a deployment importing a segmented subtopology needs a ground framer that does.

## Limitations

- Duplicate or reordered segments of equal total length are not detected (232.0-B-4 has no per-MAP sequence numbering without COP-1); ground must send in order.
- Blocking (several packets in one FDU) is not supported; such an FDU is rejected with `LengthMismatch`.
- A held partial after a mid-packet loss is released only by the next sequence on that MAP; there is no timeout and no operator reset.
- `MaxPacketSize` bounds the largest uplinkable packet; a larger packet is rejected at the first segment whose declared length exceeds it.

## Unit Testing

`test/ut` connects `allocate`/`deallocate` to a real `Svc::BufferManager` sized `PoolBufferCount × MaxPacketSize`. GTest cases cover every row of the state machine and every path above (including allocation failures by pool exhaustion and by injected invalid/short buffers, and the held-buffer model). `test/ut/Rules` holds an STest rule-based scenario (rules `SendFirst`, `SendContinuing`, `SendLast`, `SendUnsegmented`, `SendOrphan`, `SendInvalidMap`, `SendEmpty`, `SendOversize`, `SendShAbsent`, `ReturnPacket`, `ExhaustPool`) run for 10 000 seeded steps against a shadow model, checking after every step: bounds, no leak (pool allocations equal partials plus delivered-not-returned packets), no state change on rejection, exactly one synchronous frame return per `dataIn`, delivered bytes and declared length, counter accounting, and pool high-water ≤ `PoolBufferCount`.
