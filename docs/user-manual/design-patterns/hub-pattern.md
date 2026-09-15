# A Quick Look at the Hub Pattern

The F´ hub pattern connects components across a boundary while preserving the
ordinary typed-port model inside each deployment. The boundary may be an
address-space boundary between processes, a platform or processor boundary, or
a network or hardware transport. Instead of requiring every component to know
about that transport, a pair of hubs serializes calls on one side and
deserializes them into typed port calls on the other side.

![Hub Pattern](../../img/data_model6.png)

**Figure 9. Hub pattern.** Each hub instance connects to a remote node.
Connections may use sockets, ARINC 653 channels, hardware buses, UARTs, shared
memory, or another buffer-based transport.

## How it works

The basic arrangement is:

```text
    Component A1 -->--+       +-->-- Component B1
                       |       |
                       Hub A ~~> Hub B
                       |       |
    Component A2 -->--+       +-->-- Component B2
```

The `~~>` is the transport between deployments. In a typical implementation,
each hub is paired with a buffer driver:

```text
    FSW --> GenericHub --> Driver ~~> Driver --> GenericHub --> FSW
```

On the sending side, the hub allocates a buffer, writes a message-type
discriminator, port index, payload size, and serialized payload, and gives the
buffer to the driver. The remote driver gives the buffer to its hub, which
validates and deserializes it before invoking the corresponding typed output
port.

## What can cross the hub?

- **Serial data:** typed port calls whose arguments are serialized by value.
- **Events:** event ID, time tag, severity, and event arguments.
- **Telemetry:** channel ID, time tag, and telemetry value.
- **Commands:** remote command dispatches and command responses through the
  command splitter and dispatcher interfaces. See the [GenericHub SDD](../../../Svc/GenericHub/docs/sdd.md)
  for the current command-response limitation.

> **Do not pass an `Fw::Buffer` across a hub.** An `Fw::Buffer` is essentially a
> fat pointer: an address into a local address space plus a size. A hub connects
> components across a system boundary — a separate address space, processor, or
> transport — where that address is meaningless. In practice it almost never
> makes sense to send an `Fw::Buffer` through a hub; serialize and send the
> underlying data instead.

## Rules for using the pattern

- Configure both hubs with matching array sizes. Hub A's inputs must correspond
  to hub B's outputs, and hub A's outputs must correspond to hub B's inputs.
- Never pass pointers through a hub. A pointer (including an `Fw::Buffer`, which
  is effectively a fat pointer) is valid only in the address space that owns the
  pointed-to object; send serialized values instead.
- Wire received event and telemetry outputs to the deployment's event manager
  and telemetry database. A hub transports events and telemetry, but is not
  itself an event source or telemetry database.
- Use a buffer driver at each end of the transport. The
  `Drv::ByteStreamBufferAdapter` can pair a byte-stream driver with the
  buffer-driver interface expected by GenericHub when each received buffer
  carries exactly one hub message. When the transport is a raw byte stream
  (e.g. a UART), place the standard framing stack between the hub and the
  driver instead (see below).

## Putting it together

A common layout is:

```text
Deployment A                         Deployment B
-------------                        -------------
FSW -> GenericHub -> transport -> GenericHub -> FSW
```

### Framed byte-stream transport

Over a raw byte stream, hub messages must be framed so the receiving end can
recover message boundaries. No new component is needed: the existing
communication stack already speaks both the hub's `Fw.BufferSend` interface
and the framing stack's `Svc.ComDataWithContext` interface.
[`Svc::ComQueue`](../../../Svc/ComQueue/docs/sdd.md) takes the hub's outgoing
buffers on `bufferQueueIn` and feeds the framer;
[`Svc::PassThroughRouter`](../../../Svc/PassThroughRouter/docs/sdd.md) takes
the deframer's output and delivers it to the hub:

```text
GenericHub -> ComQueue         -> FprimeFramer   -> ComStub -> ByteStreamDriver ~~> (peer, mirrored)
GenericHub <- PassThroughRouter <- FprimeDeframer <- FrameAccumulator <- ComStub <- ByteStreamDriver
```

> [!IMPORTANT]
> Use F Prime framing (`Svc::FprimeFramer` / `Svc::FprimeDeframer`) for the
> hub link. `ComQueue` fills the frame context's APID from the first word of
> each buffer, which for a hub buffer is the hub message type rather than a
> packet descriptor. `FprimeFramer` ignores the APID; the CCSDS
> `TmFramer`/`AosFramer` are not intended for this use case.

`ComQueue` is the [Communication Adapter Protocol](../../reference/communication-adapter-interface.md)
client the framing stack expects: it keeps one frame in flight and releases the
next hub buffer only after the framer reports `comStatus` SUCCESS. When the
driver reports a send failure (peer gone, UART error), `ComStub` enters a
reinitialize state and asserts if another frame arrives before the driver
reconnects; `ComQueue` holds outgoing hub buffers across that outage and drains
them when SUCCESS is reported again. The one message being sent when the link
failed is lost; everything queued after it is delivered on reconnect.

Configure `ComQueue` with a single `Fw::Buffer` queue (leave the `Fw::Com`
queues at depth 0) and size its depth for the number of hub sends that may
accumulate during an outage. The hub's `Fw::Buffer` pool must cover that queue
depth plus one in flight: `ComQueue` stores buffer handles, not copies, and
returns each buffer to the hub through `bufferReturnOut` once the framer is done
with it. On overflow the incoming buffer is returned to the hub immediately
(default `QUEUE_DROP_NEWEST`) and a `QueueOverflow` event is emitted; hub
traffic is best-effort.

```fpp
# GenericHub -> ComQueue -> framer
hub.toBufferDriver          -> hubComQueue.bufferQueueIn[0]
hubComQueue.bufferReturnOut[0] -> hub.toBufferDriverReturn
hubComQueue.dataOut         -> hubFramer.dataIn
hubFramer.dataReturnOut     -> hubComQueue.dataReturnIn
hubFramer.comStatusOut      -> hubComQueue.comStatusIn
rateGroup.RateGroupMemberOut[n] -> hubComQueue.run

# deframer -> PassThroughRouter -> GenericHub
hubDeframer.dataOut         -> hubRouter.dataIn
hubRouter.dataReturnOut     -> hubDeframer.dataReturnIn
hubRouter.allPacketsOut     -> hub.fromBufferDriver
hub.fromBufferDriverReturn  -> hubRouter.allPacketsReturnIn
```

```cpp
Svc::ComQueue::QueueConfigurationTable hubQueueTable;  // all depths default to 0
hubQueueTable.entries[Svc::ComQueue::COM_PORT_COUNT + 0].depth = HUB_QUEUE_DEPTH;
hubQueueTable.entries[Svc::ComQueue::COM_PORT_COUNT + 0].priority = 0;
hubComQueue.configure(hubQueueTable, 0, allocator);
```

The framer, deframer, frame accumulator, and `ComStub` are wired to the com
driver as in the [ComFprime subtopology](../../../Svc/Subtopologies/ComFprime/docs/sdd.md)
(`comStub.comStatusOut -> framer.comStatusIn`, etc.), but must be dedicated to
the hub rather than shared with the deployment's own downlink/uplink stack: the
hub link carries hub messages, not ground packets.

For a runnable worked example, see
[`fprime-community/fprime-generic-hub-reference`](https://github.com/fprime-community/fprime-generic-hub-reference).
Its [`docs/setup.md`](https://github.com/fprime-community/fprime-generic-hub-reference/blob/devel/docs/setup.md)
walks through building both deployments and running `HubMessageTest` for serial,
buffer, event, and telemetry round trips, plus `HubCommandTest` for
cross-deployment commanding.

## Where to go next

- [GenericHub SDD](../../../Svc/GenericHub/docs/sdd.md)
- [Generic Hub reference repository](https://github.com/fprime-community/fprime-generic-hub-reference)
- [Running F´ on multiple cores](../framework/run-multi-core.md)
