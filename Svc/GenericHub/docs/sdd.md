# Svc::GenericHub Generic Hub Component

The [`Svc::GenericHub`](../GenericHub.fpp) component implements the F´ hub
pattern. A hub makes logical F´ port connections span two deployments, address
spaces, processors, or other transport boundaries. A pair of hubs multiplexes
many logical connections through a buffer-based transport:

```text
    FSW --> GenericHub --> Driver ~~> Driver --> GenericHub --> FSW
```

The `~~>` represents the deployment-to-deployment transport. It may be shared
memory, an inter-process channel, a network connection, or another mechanism.
The GenericHub is transport-independent; its driver interface operates on
`Fw::Buffer` objects.

For example, logical connections between components in deployments A and B:

```text
    A1 -->--+       +-->-- B1
            |       |
            HA ~~> HB
            |       |
    A2 -->--+       +-->-- B2
```

The driver paired with each hub must be a **buffer driver**: a combination of
components that sends and receives `Fw::Buffer` objects across the transport.
The reference implementation uses a `Drv::ByteStreamDriver` together with a
`Drv::ByteStreamBufferAdapter`. Other transport-specific drivers may be used
provided that they implement the buffer-driver interfaces described below.

## Design

The GenericHub has four interfaces:

1. ports from FSW into the hub for data that the hub sends;
2. ports from the hub into the buffer driver;
3. ports from the buffer driver into the hub; and
4. ports from the hub into FSW for received data.

The hub is bidirectional. The input configuration on one hub must be parallel
with the output configuration on its peer, and vice versa. A serial port call
is serialized by value; an `Fw::Buffer` is copied or transferred according to
the buffer lifecycle described later. A pointer must never be passed through
the hub: a pointer is meaningful only in the address space that allocated the
pointed-to object.

GenericHub is passive. Its only state between calls is the atomic
`NumAllocationFailures` counter and the `BufferAllocationFailed` throttle. Each handler runs on
its caller's thread, and several producers may be in the send path at once, so
the component connected to `toBufferDriver` must accept concurrent calls (for
example, the asynchronous inputs of `Svc::ComQueue`).

## Port catalog

### 1. FSW to hub: send interface

These ports accept data from the local deployment and serialize it for the
remote hub.

| Port | Type | Purpose |
|---|---|---|
| `eventIn` | `Fw.Log` | Sends an event to the remote deployment. |
| `tlmIn` | `Fw.Tlm` | Sends a telemetry value to the remote deployment. |
| `serialIn` | `[GenericHubCfg.NumSerialInputPorts] serial` | Sends typed, by-value serialized port calls. Do not connect ports that emit `Fw.Buffer`; use `bufferIn` for those. |
| `bufferIn` | `[GenericHubCfg.NumBufferInputPorts] Fw.BufferSend` | Sends buffer data to the remote deployment. The hub copies the data and returns the incoming buffer. |
| `bufferInReturn` | `[GenericHubCfg.NumBufferInputPorts] Fw.BufferSend` | Returns buffers received on `bufferIn` to their senders. It must match `bufferIn`. |
| `cmdDispIn` | `[CmdDispatcherSequencePorts] Fw.Com` | Accepts a command forwarded by a command splitter. |
| `cmdRespOut` | `[CmdDispatcherSequencePorts] Fw.CmdResponse` | Emits a command response received from the remote hub. |

Typical connections include:

```text
eventProducer.eventOut       -> genericHub.eventIn
telemetryProducer.tlmOut     -> genericHub.tlmIn
valueProducer.valueOut[0]    -> genericHub.serialIn[0]
bufferProducer.bufferOut      -> genericHub.bufferIn[0]
genericHub.bufferInReturn[0]  -> bufferProducer.bufferIn
commandSplitter.RemoteCmd[0] -> genericHub.cmdDispIn[0]
genericHub.cmdRespOut[0]      -> commandSplitter.seqCmdStatus[0]
```

### 2. Hub to buffer driver: send interface

The hub sends serialized messages through the buffer driver using imported
interfaces:

| Port | Type | Purpose |
|---|---|---|
| `allocate` | `Fw.BufferGet` | Requests an output buffer from the buffer manager. |
| `deallocate` | `Fw.BufferSend` | Returns an output buffer to the buffer manager. |
| `toBufferDriver` | `Fw.BufferSend` | Sends a serialized message buffer to the driver. |
| `toBufferDriverReturn` | `Fw.BufferSend` | Receives ownership back when the driver returns a sent buffer. |

The usual connections are:

```text
genericHub.allocate          -> bufferManager.bufferGetCallee
genericHub.toBufferDriver    -> bufferDriver.bufferIn
bufferDriver.bufferInReturn  -> genericHub.toBufferDriverReturn
genericHub.deallocate        -> bufferManager.bufferSendIn
```

`allocate` and `deallocate` come from `Svc.BufferAllocation`.
`toBufferDriver` and `toBufferDriverReturn` come from
`Drv.PassiveBufferDriverClientSend`.

### 2a. Hub health: scheduling, events, and telemetry

| Port | Type | Purpose |
|---|---|---|
| `schedIn` | `Svc.Sched` | Rate-group input; emits the hub's own telemetry. |
| `Log`, `LogText`, `Time`, `Tlm` | special ports | The hub's own events, text events, time, and telemetry. |

These ports report the hub's health; they are distinct from the pass-through
`eventIn`/`eventOut` and `tlmIn`/`tlmOut` ports. `schedIn` should be driven
from a rate group whenever the hub's event and telemetry ports are wired
through the hub itself (see [Allocation failure](#allocation-failure)).

### 3. Buffer driver to hub: receive interface

| Port | Type | Purpose |
|---|---|---|
| `fromBufferDriver` | `Fw.BufferSend` | Receives a serialized message buffer from the driver. |
| `fromBufferDriverReturn` | `Fw.BufferSend` | Returns a received buffer to the driver after the hub and its receiver are done with it. |

These ports come from `Drv.PassiveBufferDriverClientRecv`:

```text
bufferDriver.bufferOut             -> genericHub.fromBufferDriver
genericHub.fromBufferDriverReturn -> bufferDriver.bufferOutReturn
```

### 4. Hub to FSW: receive interface

These ports deserialize a message received from the remote hub and invoke the
corresponding local FSW port.

| Port | Type | Purpose |
|---|---|---|
| `eventOut` | `Fw.Log` | Emits a received event. |
| `tlmOut` | `Fw.Tlm` | Emits a received telemetry value. |
| `serialOut` | `[GenericHubCfg.NumSerialOutputPorts] serial` | Emits a received typed, by-value port call. |
| `bufferOut` | `[GenericHubCfg.NumBufferOutputPorts] Fw.BufferSend` | Emits a received buffer with metadata adjusted to the payload. |
| `bufferOutReturn` | `[GenericHubCfg.NumBufferOutputPorts] Fw.BufferSend` | Returns a buffer emitted on `bufferOut`. It must match `bufferOut`. |
| `cmdDispOut` | `[CmdDispatcherSequencePorts] Fw.Com` | Forwards a received command to the local command dispatcher. |
| `cmdRespIn` | `[CmdDispatcherSequencePorts] Fw.CmdResponse` | Accepts a response from the local command dispatcher for transport back to the remote hub. |

Typical connections include:

```text
genericHub.eventOut                -> eventManager.eventIn
genericHub.tlmOut                  -> tlmDb.tlmIn
genericHub.serialOut[0]            -> valueConsumer.valueIn[0]
genericHub.bufferOut[0]            -> bufferConsumer.bufferIn
bufferConsumer.bufferInReturn     -> genericHub.bufferOutReturn[0]
genericHub.cmdDispOut[0]           -> commandDispatcher.seqCmdBuff[0]
commandDispatcher.seqCmdStatus[0] -> genericHub.cmdRespIn[0]
```

## Message framing and behavior

Every message sent to the buffer driver has this outer framing:

1. a `U32` message-type discriminator;
2. a `U32` port index;
3. an `FwBuffSizeType` payload size; and
4. the payload bytes.

The hub allocates enough space for these fields and the payload, serializes the
fields in that order, sets the buffer size to the serialized size, and emits
the buffer on `toBufferDriver`. On receipt, the hub validates the type,
deserializes the port and payload size, checks that the payload size matches
the received buffer, and dispatches according to the message type.

| Message type | Payload and behavior |
|---|---|
| Serial (`HUB_TYPE_PORT`) | The payload is the serialized by-value argument list for `serialIn`. The receiver wraps the payload in an `Fw::ExternalSerializeBuffer` and invokes the matching `serialOut` port. |
| Buffer (`HUB_TYPE_BUFFER`) | The sender copies the incoming buffer data into a newly allocated transport buffer. The receiver adjusts the received buffer metadata to the payload and emits it on the matching `bufferOut` port. |
| Event (`HUB_TYPE_EVENT`) | The payload contains the event ID, time tag, severity, and `Fw::LogBuffer` arguments. The receiver deserializes these values and invokes `eventOut`. |
| Telemetry (`HUB_TYPE_CHANNEL`) | The payload contains the channel ID, time tag, and `Fw::TlmBuffer` value. The receiver deserializes these values and invokes `tlmOut`. |
| Command dispatch (`HUB_TYPE_CMD_DISP`) | The payload contains the serialized `Fw::ComBuffer` command data followed by its `U32` context. The receiver invokes the matching `cmdDispOut` port. |
| Command response (`HUB_TYPE_CMD_RESP`) | The payload contains the opcode, command sequence, and `Fw::CmdResponse`. The receiver invokes the matching `cmdRespOut` port. |

Malformed or invalid messages are not dispatched, and the received transport
buffer is returned to the driver. A message whose destination port is invalid
or disconnected is also not dispatched; buffer messages in that case are
returned to the driver to avoid a leak.

## Command routing

The command ports allow a command splitter to forward a command from one
deployment to another:

```text
command splitter A
    -> hub A.cmdDispIn
    -> buffer driver / transport
    -> hub B.cmdDispOut
    -> command dispatcher B
    -> remote component B
```

The command dispatcher response follows the reverse path:

```text
command dispatcher B
    -> hub B.cmdRespIn
    -> buffer driver / transport
    -> hub A.cmdRespOut
    -> command splitter A
```

The command arrays are sized by the framework constant
`CmdDispatcherSequencePorts`, which is `5` in the default configuration.

The command port arrays are not declared as matched, and the hub carries only
the port index. A response reaches its originator only if the indices are wired
consistently: `cmdDispIn[k]` and `cmdRespOut[k]` on hub A must pair with
`cmdDispOut[k]` and `cmdRespIn[k]` on hub B. `Svc::CmdDispatcher` declares
`match seqCmdStatus with seqCmdBuff`, so connecting more than one of its slots
to the same hub requires `unmatched` connections:

```text
unmatched hub.cmdDispOut[0] -> cmdDisp.seqCmdBuff[2]
unmatched cmdDisp.seqCmdStatus[2] -> hub.cmdRespIn[0]
```

### Known limitation

Although GenericHub serializes and forwards command responses, the standard
CCSDS routing path currently drops the response at the origin deployment.
`Svc::FprimeRouter::cmdResponseIn_handler` is a no-op (`// Nothing to do`).
Consequently, command completion is not observable at the origin deployment's
GDS through that path today. DeploymentB can still dispatch and complete the
command locally.

## Buffer lifecycle

For a locally produced message, the hub:

1. requests a transport buffer through `allocate`;
2. serializes the message type, port index, payload size, and payload;
3. emits the buffer through `toBufferDriver`;
4. receives ownership back through `toBufferDriverReturn`; and
5. returns the buffer to the manager through `deallocate`.

### Allocation failure

If `allocate` returns an invalid buffer (null data, or smaller than the framed
message), the hub drops the message: it returns a non-null undersized buffer
through `deallocate`, increments the `NumAllocationFailures` counter, and emits
the `BufferAllocationFailed` event (throttled to 5) with the message type, port
index, and requested size. Buffers received on `bufferIn` are still returned
through `bufferInReturn`. The hub remains operational and resumes sending once
allocation succeeds. A dropped `cmdDispIn` command is failed back to its caller
with `Fw::CmdResponse::EXECUTION_ERROR` on the matching `cmdRespOut` port; a
dropped `cmdRespIn` response cannot be reported to the far side and is lost;
the originating `Svc::CmdDispatcher` has no response timeout, so each loss
permanently occupies one of its sequence-tracker slots. Size the pool so that
`cmdRespIn` never drops.

The counter is read from any caller thread (`std::atomic`) and written to
telemetry only from `schedIn`. Emitting telemetry on the failure path would
re-enter the hub whenever the hub is the deployment's telemetry sink, so the
`schedIn` port is required for the channel to be observable. The event is
emitted on the failure path; when the hub is also the deployment's event sink
(`event connections instance hub` routes `hub.Log` back into `hub.eventIn`),
each failure re-enters the hub once per event until the throttle (5) is
reached, after which failures are only counted. Existing deployments must add
the `schedIn` connection to see the counter.

Size the pool for the worst case: the capacity of every queue downstream of
`toBufferDriver`, plus buffers held by the driver, plus one per thread that
can call the hub's input ports concurrently.

For `bufferIn`, the hub copies the incoming data into the newly allocated
transport buffer and immediately returns the original buffer through the
matching `bufferInReturn` port. The hub never sends the original pointer
through the transport.

For `serialIn`, `eventIn`, and `tlmIn`, the remote hub deserializes the payload
by value, invokes the matching output port, and returns the received transport
buffer. For `bufferOut`, the remote hub reuses the received storage by
adjusting its metadata to the payload. Ownership remains with the receiving
consumer until it invokes the matching `bufferOutReturn` port; the hub then
returns the transport buffer through `fromBufferDriverReturn`.

Pointers must never be passed through a GenericHub. An address is valid only
in the address space that owns it, so a pointer transmitted across the hub
would refer to invalid or unrelated memory in the receiving deployment.

## Configuration

The default GenericHub configuration is defined in
`default/config/GenericHubCfg.fpp`:

```text
Svc.GenericHubCfg.NumSerialInputPorts  = 10
Svc.GenericHubCfg.NumBufferInputPorts  = 10
Svc.GenericHubCfg.NumSerialOutputPorts = 10
Svc.GenericHubCfg.NumBufferOutputPorts = 10
```

The command ports use the framework constant
`CmdDispatcherSequencePorts`, defined in `default/config/AcConstants.fpp`
and set to `5` by default.

Both deployments must use matching settings. In particular, the inputs on hub
A must match the outputs on hub B, and the outputs on hub A must match the
inputs on hub B. The port index is part of the serialized message, so the
corresponding arrays must be wired in parallel.

The serialized message carries no version field. Both deployments must also use
the same serialized framework types and limits, including `FwBuffSizeType`,
`FwOpcodeType`, `FwEventIdType`, `FwChanIdType`, the `Fw::Time` fields, and
`FW_COM_BUFFER_MAX_SIZE`. GenericHub requires `FW_PORT_SERIALIZATION`.

To use the event and telemetry pattern connections, include the hub in the
corresponding topology specifiers:

```text
event connections instance hub
telemetry connections instance hub
```

## Idiosyncrasies

The buffer driver can report send and receive errors, but GenericHub currently
drops those errors. Projects that need to monitor transport failures must add
that handling around or within the component. Received messages with an
invalid type, size, or port index, or for an unconnected output port, are
dropped without an event or telemetry.

Outgoing messages are dropped when the allocator returns an invalid buffer
(for example, `Svc::BufferManager` with no free buffer of sufficient size); see
[Allocation failure](#allocation-failure).

Apart from its own `BufferAllocationFailed` event and `NumAllocationFailures`
channel, the hub is not an event source or telemetry database. Its `eventOut`
and `tlmOut` ports still need to be wired to the deployment's event manager and
telemetry database, respectively.

## Requirements

| Name | Description | Validation |
|---|---|---|
| GENHUB-001 | The GenericHub shall receive incoming port and buffer calls. | Unit test |
| GENHUB-002 | The GenericHub shall serialize incoming port and buffer calls to an output buffer-driver port. | Unit test |
| GENHUB-003 | The GenericHub shall deserialize incoming transport buffers to output port and buffer calls. | Unit test |
| GENHUB-004 | The GenericHub shall work with another GenericHub to send port and buffer calls. | Unit test |
| GENHUB-005 | The GenericHub shall forward command dispatches and command responses between deployments. | Unit test |
| GENHUB-006 | When `allocate` returns an invalid buffer (null data, or size less than requested), the GenericHub shall drop the outgoing message instead of asserting and shall remain operational. | Unit test |
| GENHUB-007 | On such a drop, the GenericHub shall emit a `WARNING_HI` event (throttled to 5) identifying the message type, port index, and requested size. | Unit test |
| GENHUB-008 | The GenericHub shall count allocation failures and report the count as a telemetry channel from `schedIn`. | Unit test, integration test (fprime-generic-hub-reference) |
| GENHUB-009 | An undersized but valid buffer returned by `allocate` shall be returned through `deallocate`. | Unit test |
| GENHUB-010 | Buffers received on `bufferIn` shall be returned through `bufferInReturn` whether or not the outgoing message is dropped. | Unit test |
| GENHUB-011 | A `cmdDispIn` command dropped on allocation failure shall be failed back to its caller with `EXECUTION_ERROR` on the matching `cmdRespOut` port. | Unit test |

## Example formations

The following diagrams show progressively more detail:

![Top Level Generic Hub](./img/gh-top.png)

![Top Level Generic Hub with buffers](./img/gh-top-buff.png)

![Generic Hub input-side connections](./img/gh-left.png)

![Generic Hub output-side connections](./img/gh-right.png)

## Change Log

| Date | Description |
|---|---|
| 2020-12-21 | Initial Draft |
| 2021-01-29 | Updated |
| 2023-06-09 | Added telemetry and event helpers |
| 2026-07-09 | Documented command routing, current configuration constants, buffer-driver interfaces, framing, and buffer lifecycle |
| 2026-09-29 | Documented threading, command index pairing, cross-deployment type agreement, silent receive drops, and allocation failure |
| 2026-10-11 | Allocation failure drops the message with an event and counter instead of asserting; added `schedIn` and the hub's own event/telemetry ports |
