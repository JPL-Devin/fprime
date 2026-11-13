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
  and telemetry database. A hub transports events and telemetry; apart from
  its own `BufferAllocationFailed` event and `NumAllocationFailures` channel,
  it is not an event source or telemetry database.
- Use a buffer driver at each end of the transport, and wire the send side
  through `Svc.ComQueue` and `Svc.ComStub` (see
  [Recommended wiring](#recommended-wiring)). `Drv::ByteStreamBufferAdapter`
  pairs a byte-stream driver's receive side with the hub.
- Use a transport that preserves message boundaries. The hub expects one
  message per received buffer and does not reassemble streams: `Drv.Udp`
  does this natively; TCP and UART links need the standard F´ framing
  (`Svc.FprimeFramer` on the send side, `Svc.FrameAccumulator` +
  `Svc.FprimeDeframer` on the receive side) and a small receive-side adapter,
  as described in the [GenericHub SDD](../../../Svc/GenericHub/docs/sdd.md#transport-requirements).
- Wire command ports with the same indices on both hubs; the index is how a
  command response finds its originator.
- Size the hub's buffer pool for the worst case. When allocation fails,
  GenericHub drops the message, emits `BufferAllocationFailed` (once per
  failure episode, re-armed by the next successful send) and counts every
  drop in `NumAllocationFailures`; drive `schedIn` from a rate group so the counter
  is reported (deployments written before `schedIn` existed must add this
  connection). A dropped `cmdDispIn` command is failed back to its caller with
  `EXECUTION_ERROR`.
- Monitor the link outside the hub. GenericHub reports no transport errors and
  silently drops invalid received messages; `Svc.ComQueue` reports queue depth
  and overflow, `Drv::ByteStreamBufferAdapter` reports receive errors. In the
  recommended wiring a failed send is not evented (see the SDD's residual-loss
  note).

## Recommended wiring

The hub is passive and never queues: when it cannot get a buffer it drops the
message, emits `BufferAllocationFailed` and counts the drop. Buffering across
a link outage belongs between the hub and the driver. The recommended send
path reuses the framework's communication-adapter components:

```text
producers -> GenericHub.toBufferDriver -> Svc.ComQueue -> Svc.ComStub -> driver
             GenericHub.toBufferDriverReturn <- Svc.ComQueue.bufferReturnOut
             Svc.ComStub.drvConnected <- driver.ready
```

`Svc.ComQueue` takes the buffer on its own thread, so producers never block on
the socket; it holds frames while the driver cannot send and resumes when the
driver re-announces readiness. Size the queue depth to at least the hub's
buffer count (`D >= N`, instance queue `Q >= N + 8`) so that the hub remains
the single drop point. The receive side stays `driver -> adapter -> GenericHub`.
Over `Drv.Udp` a failed send stalls this path until the driver reconnects (the
reference deployment accepts that: UDP needs no framing, and the stall is
visible as `NumAllocationFailures` rising while `buffQueueDepth` sits at the
queue depth); TCP and UART recover after a reconnect but need F´ framing plus a
receive-side adapter that the framework does not provide. Details, the full
connection table, and the sizing rule are in the
[GenericHub SDD](../../../Svc/GenericHub/docs/sdd.md#transport-formations).

## Putting it together

A common layout is:

```text
Deployment A                                           Deployment B
-------------                                          -------------
FSW -> GenericHub -> ComQueue -> ComStub -> transport -> GenericHub -> FSW
```

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
