
# Buffer Management Functionality

## References

- [Fw::Buffer SDD](https://github.com/nasa/fprime/blob/devel/Fw/Buffer/docs/sdd.md)
- [F Prime BufferManager SDD](https://github.com/nasa/fprime/blob/devel/Svc/BufferManager/docs/sdd.md)
- [F Prime BufferAccumulator SDD](https://github.com/nasa/fprime/blob/devel/Svc/BufferAccumulator/docs/sdd.md)
- [F Prime BufferRepeater SDD](https://github.com/nasa/fprime/blob/devel/Svc/BufferRepeater/docs/sdd.md)
- [F Prime BufferLogger](https://github.com/nasa/fprime/blob/devel/Svc/BufferLogger/BufferLogger.fpp)
- [F Prime StaticMemory SDD](https://github.com/nasa/fprime/blob/devel/Svc/StaticMemory/docs/sdd.md)
- [F Prime TcMapReassembler SDD](https://github.com/nasa/fprime/blob/devel/Svc/Ccsds/TcMapReassembler/docs/sdd.md)
- [ComCcsds Subtopology SDD](https://github.com/nasa/fprime/blob/devel/Svc/Subtopologies/ComCcsds/docs/sdd.md)

## Overview

Buffer management provides memory allocation, distribution, and lifecycle services for variable-size data buffers used throughout the system. Components that need temporary memory for communication data, file transfers, or data products request buffers from a buffer manager, use them, and return them when done. Additional utility components provide buffer accumulation, replication, and logging.

### Buffer Allocation

Two buffer allocation strategies are available:

- **Buffer Manager** — Implements a buffer pool with dynamically managed memory. The pool is configured at setup time with bin sizes and counts, allowing efficient allocation of buffers at various sizes without per-allocation heap calls.

- **Static Memory** — Allocates buffers from statically defined memory regions. Each output port corresponds to a fixed-size memory region. This is useful for supporting components that need buffer management when only one outstanding allocation is needed at a time, avoiding the complexity of a full buffer pool.

In both cases, components request buffers via the Fw::BufferGet port and return them via the Fw::BufferSend port.

#### Dedicated pools

A Buffer Manager pool is shared by every component connected to it, and a request is served by the **first bin whose buffer size fits** the request. Two consumers with different lifetimes on one pool therefore interfere: buffers held for a long time by one consumer can starve the other, and a burst on one path can drain a bin the other path relies on. When a consumer can hold buffers indefinitely, give it its **own** `Svc.BufferManager` instance with its own manager ID.

The segmented CCSDS uplink (`ComCcsds.SegmentedSubtopology`, see the [ComCcsds subtopology](subtopology-com-ccsds.md)) is the reference example: the TC MAP Reassembler holds one buffer per MAP channel for as long as a Space Packet is being reassembled from several TC frames (until the LAST segment, an abandon, or an overflow — there is no timeout), which is unrelated to the per-frame lifetime of the communications pool (`commsBufferManager`). It therefore allocates from a dedicated `tcPacketBufferManager`, configured with a **single bin** from the `TcMapCfg` constants:

| Quantity | Value | Meaning |
| --- | --- | --- |
| buffer size | `TcMapCfg.MaxPacketSize` (default 4096) | largest reassembled Space Packet accepted |
| buffer count | `TcMapCfg.PoolBufferCount = MapChannelCount + MaxPacketsInFlight` (default 1 + 4 = 5) | one buffer per MAP in progress plus the packets delivered downstream and not yet returned |
| backing memory | `TcMapCfg.PoolBytes = PoolBufferCount * MaxPacketSize` (default 20 480 octets, from `ComCcsds::Allocation::memAllocator`) | |
| manager ID | `TcMapCfg.PoolManagerId` (201) | distinct from the communications pool's ID |

The generated setup code checks these relations at compile time (`static_assert`), so a configuration whose pool cannot hold one packet per MAP plus `MaxPacketsInFlight` does not build. With `MaxPacketSize = 65542` (the largest Space Packet) the default pool is 5 × 65 542 = 327 710 octets. Ownership is **copy-always**: each frame's portion is copied into the pool buffer and the frame buffer is returned upstream immediately; the pool buffer is handed to the Space Packet Deframer with the completed packet and returned to the pool through `dataReturnIn`. When the pool is exhausted the reassembler drops the frame with an `AllocationFailed` event and allocates nothing; the communications pool is never touched.

### Buffer Accumulation

The Buffer Accumulator accepts incoming buffers and queues them for later processing. This is useful when the data arrival rate may temporarily exceed the processing rate, or to pause buffer processing during critical events. Buffers are stored in an internal queue and drained in order. If the queue fills, the accumulator can either drop the newest buffer or assert, depending on the configuration. Operators can pause and resume buffer draining via command.

### Buffer Replication

The Buffer Repeater takes an incoming buffer and replicates it to multiple output ports. This is useful when the same data needs to be consumed by multiple downstream components simultaneously.

### Buffer Logging

The Buffer Logger writes incoming buffers to files on the file system. It is typically connected in the communication path to record all transmitted or received data for post-analysis. Log files are rotated based on a configurable size limit.

### Off Nominal

- If the Buffer Manager runs out of available buffers in the requested size bin, it returns an empty (invalid) buffer. The requesting component must check validity before use.
- The Buffer Accumulator reports a warning when its queue is full and a buffer must be dropped.
- Buffers that are never returned consume pool capacity. The Buffer Manager tracks outstanding allocations via telemetry.
- A dedicated pool bounds the effect of a stalled consumer: a MAP whose reassembly never completes holds at most one buffer of its own pool, and exhaustion of that pool drops only the frames that would need a new reassembly buffer.
