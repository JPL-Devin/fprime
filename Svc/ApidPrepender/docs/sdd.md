# Svc::ApidPrepender

The `Svc::ApidPrepender` component serializes a buffer's APID (packet type) into the head of a new buffer so that the packet type survives transit over APID-agnostic `Fw.BufferSend` links, such as the [GenericHub pattern](../../GenericHub/docs/sdd.md). It is intended to be paired with an [Svc::ApidStripper](../../ApidStripper/docs/sdd.md) component on the receiving side, which recovers the APID and re-emits it as an explicit port argument.

On each `dataIn` invocation, `Svc::ApidPrepender` allocates a new buffer of size `sizeof(FwPacketDescriptorType)` plus the payload size, serializes the APID followed by the payload into it, emits it on `dataOut`, and returns ownership of the original buffer on `dataReturnOut`. Buffers emitted on `dataOut` are expected to be returned on `dataOutReturn`, where they are deallocated.

Example topology (remote node sending producer traffic across a hub):

```
fileDownlink.bufferSendOut -> apidPrepender.dataIn
apidPrepender.dataReturnOut -> fileDownlink.bufferReturn
apidPrepender.dataOut -> hub.bufferIn[0]
hub.bufferInReturn[0] -> apidPrepender.dataOutReturn
```

## Port Descriptions

| Kind | Name | Type | Description |
|---|---|---|---|
| `sync input` | `dataIn` | `Fw.BufferWithApid` | Receiving data with its APID |
| `output` | `dataReturnOut` | `Fw.BufferSend` | Returning ownership of buffers received on `dataIn` to their sender |
| `output` | `dataOut` | `Fw.BufferSend` | Emitting the APID-prepended buffer |
| `sync input` | `dataOutReturn` | `Fw.BufferSend` | Receiving back ownership of buffers emitted on `dataOut` |
| `output` | `allocate` | `Fw.BufferGet` | Allocation request |
| `output` | `deallocate` | `Fw.BufferSend` | Deallocation request |

## Requirements

| Name | Description | Validation |
|---|---|---|
| SVC-APID_PREPENDER-001 | `Svc::ApidPrepender` shall emit on `dataOut` a buffer containing the serialized APID followed by the payload received on `dataIn`. | Unit test |
| SVC-APID_PREPENDER-002 | `Svc::ApidPrepender` shall return ownership of all buffers received on `dataIn` through `dataReturnOut`. | Unit test |
| SVC-APID_PREPENDER-003 | `Svc::ApidPrepender` shall emit a warning event and drop the data when buffer allocation fails. | Unit test |
| SVC-APID_PREPENDER-004 | `Svc::ApidPrepender` shall deallocate buffers received on `dataOutReturn`. | Unit test |
