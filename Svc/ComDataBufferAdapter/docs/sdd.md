# Svc::ComDataBufferAdapter

## 1. Introduction

`Svc::ComDataBufferAdapter` is a passive component that adapts between two F´ interfaces:

1. The [`Drv.PassiveBufferDriver`](../../../Drv/Interfaces/docs/sdd.md) interface, which
   exchanges `Fw::Buffer` objects with a buffer-driver client such as
   [`Svc::GenericHub`](../../GenericHub/docs/sdd.md).
2. The `Svc.ComDataWithContext` ports used by the framing stack
   ([`Svc.Framer`, `Svc.Deframer`](../../Interfaces/docs/sdd.md)), which
   carry an `Fw::Buffer` together with a `ComCfg::FrameContext`.

Together with a framer, a deframer, and a com interface (e.g. [`Svc::ComStub`](../../ComStub/docs/sdd.md)
over a `Drv.ByteStreamDriver` such as a UART), the adapter functions as a `PassiveBufferDriver`. This lets
a hub run over a transport that does not preserve message boundaries, where
[`Drv::ByteStreamBufferAdapter`](../../../Drv/ByteStreamBufferAdapter/docs/sdd.md) alone is not sufficient.

The adapter neither allocates nor copies: every buffer is forwarded by reference and ownership follows the
return-to-sender pattern on both sides.

> [!IMPORTANT]
> Use the adapter with F Prime framing ([`Svc::FprimeFramer`](../../FprimeFramer/docs/sdd.md) and
> [`Svc::FprimeDeframer`](../../FprimeDeframer/docs/sdd.md)). The adapter applies no flow control: each
> buffer received on `bufferIn` is forwarded to the framer immediately, so the framer must accept a new input
> while previous frames are still in flight. `Svc::FprimeFramer` allocates a frame per input and does.
> The CCSDS `Svc::Ccsds::TmFramer` and `Svc::Ccsds::AosFramer` hold a single frame and assert if one is
> still outstanding; they are not supported behind this adapter.

## 2. Requirements

| Name | Description | Validation |
|------|-------------|------------|
| SVC-COM-DATA-BUFFER-ADAPTER-001 | The component shall forward buffers received on `bufferIn` on `dataOut` with the configured frame context, without copying. | Unit test |
| SVC-COM-DATA-BUFFER-ADAPTER-002 | The component shall return buffers received on `dataReturnIn` on `bufferInReturn`. | Unit test |
| SVC-COM-DATA-BUFFER-ADAPTER-003 | The component shall forward buffers received on `dataIn` on `bufferOut`, without copying. | Unit test |
| SVC-COM-DATA-BUFFER-ADAPTER-004 | The component shall return buffers received on `bufferOutReturn` on `dataReturnOut`. | Unit test |
| SVC-COM-DATA-BUFFER-ADAPTER-005 | The component shall allow the frame context sent on `dataOut` to be configured. | Unit test |

## 3. Design

### 3.1 Ports

| Kind | Name | Type | Description |
|------|------|------|-------------|
| sync input | `bufferIn` | `Fw.BufferSend` | Buffers from the client to send; forwarded on `dataOut` |
| output | `bufferInReturn` | `Fw.BufferSend` | Returns ownership of buffers received on `bufferIn` |
| output | `bufferOut` | `Fw.BufferSend` | Buffers received on `dataIn`, sent to the client |
| sync input | `bufferOutReturn` | `Fw.BufferSend` | Receives back ownership of buffers sent on `bufferOut` |
| output | `dataOut` | `Svc.ComDataWithContext` | Buffers from `bufferIn` with the configured context; connect to a framer's `dataIn` |
| sync input | `dataReturnIn` | `Svc.ComDataWithContext` | Ownership returned by the framer; forwarded on `bufferInReturn` |
| sync input | `dataIn` | `Svc.ComDataWithContext` | Deframed data from a deframer's `dataOut`; forwarded on `bufferOut` |
| output | `dataReturnOut` | `Svc.ComDataWithContext` | Returns ownership of buffers received on `dataIn` to the deframer |

The `bufferIn`/`bufferInReturn`/`bufferOut`/`bufferOutReturn` ports are imported from `Drv.PassiveBufferDriver`.

### 3.2 Behavior

Send direction (client to framer):

```text
client.toBufferDriver -> bufferIn  ==> dataOut(buffer, m_context) -> framer.dataIn
framer.dataReturnOut  -> dataReturnIn ==> bufferInReturn(buffer) -> client.toBufferDriverReturn
```

Receive direction (deframer to client):

```text
deframer.dataOut            -> dataIn          ==> bufferOut(buffer)    -> client.fromBufferDriver
client.fromBufferDriverReturn -> bufferOutReturn ==> dataReturnOut(buffer, default context) -> deframer.dataReturnIn
```

The context received on `dataIn` is not retained across the round trip; a default `ComCfg::FrameContext` is
sent on `dataReturnOut`. The framework deframers pass this context through unchanged, so this has no effect.

### 3.3 Configuration

`configure(const ComCfg::FrameContext& context)` sets the context stamped on every buffer emitted on
`dataOut`. It defaults to a default-constructed `ComCfg::FrameContext`, which is sufficient for
`Svc::FprimeFramer`. A framer that reads the context requires it to be configured in the topology
`configComponents` phase.

### 3.4 Assumptions

1. `dataOut` and `bufferOut` are always connected; the component does not check connectivity.
2. The framer and deframer connected to the adapter are dedicated to it. `dataOut`/`dataReturnIn` form a
   single ownership loop with one framer, and `dataIn`/`dataReturnOut` with one deframer, so a framer
   cannot be shared with e.g. a `Svc::ComQueue`.
3. The framer accepts a new input while previously emitted frames are still outstanding (see the note in
   Section 1). The com interface downstream of the framer (e.g. `Svc::ComStub`) still requires
   `comStatusOut` to be connected to the framer's `comStatusIn`. The framer's `comStatusOut` may be left
   unconnected: the adapter applies no flow control, matching `Svc::GenericHub`.

### 3.5 Sample topology

```text
hub.toBufferDriver          -> adapter.bufferIn
adapter.bufferInReturn      -> hub.toBufferDriverReturn
adapter.dataOut             -> framer.dataIn
framer.dataReturnOut        -> adapter.dataReturnIn

deframer.dataOut            -> adapter.dataIn
adapter.dataReturnOut       -> deframer.dataReturnIn
adapter.bufferOut           -> hub.fromBufferDriver
hub.fromBufferDriverReturn  -> adapter.bufferOutReturn
```

The framer/deframer side is wired to a com interface as described in the
[ComFprime subtopology](../../Subtopologies/ComFprime/docs/sdd.md).
