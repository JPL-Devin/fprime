# Svc::ApidStripper

The `Svc::ApidStripper` component deserializes a leading APID (packet type) from a buffer received over an APID-agnostic `Fw.BufferSend` link, such as the [GenericHub pattern](../../GenericHub/docs/sdd.md), and re-emits the remaining payload with the APID as an explicit port argument. It is intended to be paired with an [Svc::ApidPrepender](../../ApidPrepender/docs/sdd.md) component on the sending side.

On each `dataIn` invocation, `Svc::ApidStripper` deserializes the leading `FwPacketDescriptorType` value, adjusts the buffer to point past it, and emits the buffer with the APID on `dataOut`. If the descriptor value is not a valid `ComCfg::Apid` constant, the APID is set to `ComCfg::Apid::INVALID_UNINITIALIZED` and handling is left to downstream components. Buffers too small to contain an APID are dropped with a warning event and returned on `dataReturnOut`. Buffers returned on `dataReturnIn` are passed through to `dataReturnOut` with their adjusted data pointer, matching the deframer convention of returning shifted buffers to the allocator.

Example topology (main node receiving producer traffic from across a hub):

```
hub.bufferOut[0] -> apidStripper.dataIn
apidStripper.dataOut -> comQueue.bufferQueueIn[0]
comQueue.bufferReturnOut[0] -> apidStripper.dataReturnIn
apidStripper.dataReturnOut -> hub.bufferOutReturn[0]
```

## Port Descriptions

| Kind | Name | Type | Description |
|---|---|---|---|
| `sync input` | `dataIn` | `Fw.BufferSend` | Receiving APID-prepended buffers |
| `output` | `dataOut` | `Fw.BufferWithApid` | Emitting the stripped buffer with its APID |
| `sync input` | `dataReturnIn` | `Fw.BufferSend` | Receiving back ownership of buffers emitted on `dataOut` |
| `output` | `dataReturnOut` | `Fw.BufferSend` | Returning ownership of buffers received on `dataIn` to their sender |

## Requirements

| Name | Description | Validation |
|---|---|---|
| SVC-APID_STRIPPER-001 | `Svc::ApidStripper` shall emit on `dataOut` the payload of the buffer received on `dataIn` with the deserialized APID as a port argument. | Unit test |
| SVC-APID_STRIPPER-002 | `Svc::ApidStripper` shall map out-of-range descriptor values to `ComCfg::Apid::INVALID_UNINITIALIZED`. | Unit test |
| SVC-APID_STRIPPER-003 | `Svc::ApidStripper` shall drop buffers too small to contain an APID, emit a warning event, and return them on `dataReturnOut`. | Unit test |
| SVC-APID_STRIPPER-004 | `Svc::ApidStripper` shall pass buffers received on `dataReturnIn` through to `dataReturnOut`. | Unit test |
