# Svc::Ccsds::CcsdsSdlsFramer

The `Svc::Ccsds::CcsdsSdlsFramer` component frames buffers into CCSDS SDLS (Space Data Link Security) frames. It is both a framer (implementing the `Svc.Framer` interface within a framing pipeline) and an encryption client (implementing `Svc.Ccsds.CcsdsSdlsEncryptClient` toward an encryption helper such as `Svc.Ccsds.SdlsSaRouter` or an encryptor component). It is the downlink mirror of [`Svc::Ccsds::CcsdsSdlsDeframer`](../../CcsdsSdlsDeframer/docs/sdd.md).

## Introduction

Framing an SDLS frame proceeds as follows:

1. Determine the security association (SA) index: from the frame context when set, otherwise from the `SA_INDEX` parameter.
2. Record the SA index in the frame context and pass the data, SA index, and context to the encryption helper.
3. Check the status passed forward with the encrypted data; on failure raise an event and return the buffer.
4. Upon receiving successfully encrypted data back, prepend the 16-bit SA index — in place when the zero-copy contract holds (see below), otherwise into a newly allocated frame buffer.
5. Pass the SDLS frame downstream for further framing/transmission.

### Zero-copy zone path

When an upstream [`Svc::Ccsds::SppZonePacker`](../../SppZonePacker/docs/sdd.md) sends a data zone with `context.zeroCopyFrame` set, the framer checks an **in-place encryption contract** on `encryptIn`: the encryptor returned the very same buffer (same data pointer and size — ciphertext length equal to plaintext length, as with CTR/GCM-class ciphers) and the buffer carries at least 2 bytes of headroom. If the contract holds, the SA index is prepended into the headroom by advancing the buffer window backward — no allocation, no copy — and the same upstream-owned buffer flows downstream; its return on `dataReturnIn` is forwarded upstream via `dataReturnOut` instead of being deallocated. If the encryptor returns a different or resized buffer, the framer falls back to the allocate-and-copy path below and clears `zeroCopyFrame` in the downstream context. Encryptors that expand data (e.g. appending a MAC into the zone) must reserve that space inside the zone or forgo the in-place path.

Buffer ownership follows the standard F Prime data-with-context return pattern: encrypted buffers are returned to the encryption helper via `encryptReturnOut` once copied into the frame, allocated frame buffers returned from downstream (`dataReturnIn`) are deallocated via `bufferDeallocate` (zero-copy frames are instead forwarded upstream), and original data buffers returned by the helper (`bufferReturnIn`) go back upstream via `dataReturnOut`. Com status (`comStatusIn`) passes through unmodified to `comStatusOut`; additionally, when a frame is dropped (encryption failure or buffer-allocation failure), the framer emits a ready-for-more com status on `comStatusOut` so a `ComQueue`-driven downlink does not stall.

## Requirements

| Name | Description | Rationale | Validation |
|---|---|---|---|
| SVC-CCSDS-SDLS-FRAMER-001 | The CcsdsSdlsFramer shall accept data with frame context via the `Svc.Framer` interface (`dataIn`). | Standard framing pipeline entry point. | Unit test |
| SVC-CCSDS-SDLS-FRAMER-002 | The CcsdsSdlsFramer shall determine the security association (SA) index from the frame context when set, otherwise from the `SA_INDEX` parameter, record it in the frame context, and pass the data, SA index, and context to the encryption helper via `encryptOut`. | The SA index selects the encryption path; upstream components may override the configured default. | Unit test |
| SVC-CCSDS-SDLS-FRAMER-003 | Upon receiving encrypted data on `encryptIn`, the CcsdsSdlsFramer shall prepend the 16-bit SA index to the encrypted data — in place within the buffer headroom when the zero-copy in-place contract holds, otherwise into a frame buffer allocated via `bufferAllocate` with ownership of the encrypted buffer returned via `encryptReturnOut` — and pass the resulting SDLS frame downstream via `dataOut`. | The SA index must lead the frame so the receiving deframer can extract it; zero-copy zones already reserve headroom for it. | Unit test |
| SVC-CCSDS-SDLS-FRAMER-004 | Upon a non-SUCCESS status passed forward on `encryptIn`, the CcsdsSdlsFramer shall emit the `EncryptionFailed` WARNING_HI event and return ownership of the accompanying buffer to the encryption subsystem via `encryptReturnOut`, and emit a ready-for-more com status on `comStatusOut`. | Encryption failures must be visible to the system, the buffer must not leak, and a ComQueue-driven downlink must not stall. | Unit test |
| SVC-CCSDS-SDLS-FRAMER-005 | The CcsdsSdlsFramer shall deallocate allocated frame buffers received back on `dataReturnIn` via `bufferDeallocate`, and forward upstream-owned zero-copy frames upstream via `dataReturnOut`. | The framer must release buffers it allocated and return upstream buffers to their owner. | Unit test |
| SVC-CCSDS-SDLS-FRAMER-006 | The CcsdsSdlsFramer shall return original data buffers received back from the encryption helper (`bufferReturnIn`) upstream via `dataReturnOut`. | Original data buffers must return to their upstream allocator. | Unit test |
| SVC-CCSDS-SDLS-FRAMER-007 | The CcsdsSdlsFramer shall pass com status received on `comStatusIn` through to `comStatusOut` unmodified. | Ready signals must traverse the framing pipeline. | Unit test |
| SVC-CCSDS-SDLS-FRAMER-008 | Upon an invalid or undersized buffer allocation, the CcsdsSdlsFramer shall emit the `BufferAllocationFailed` WARNING_HI event, deallocate the undersized buffer when valid, return the encrypted buffer via `encryptReturnOut`, and emit a ready-for-more com status on `comStatusOut`. | Allocation failures must be reported, no buffer may leak, and a ComQueue-driven downlink must not stall; invalid buffers need not be deallocated. | Unit test |

## Design

The component is passive with no commands or telemetry. It composes two interfaces plus allocation ports:

| Kind | Name | Port Type | Description |
|---|---|---|---|
| sync input | dataIn | Svc.ComDataWithContext | Receives data to frame (from `Svc.Framer`). |
| output | dataOut | Svc.ComDataWithContext | Sends the SDLS frame downstream. |
| output | dataReturnOut | Svc.ComDataWithContext | Returns data buffer ownership upstream. |
| sync input | dataReturnIn | Svc.ComDataWithContext | Receives back ownership of frames sent on `dataOut`. |
| sync input | comStatusIn | Fw.SuccessCondition | Receives downstream ready status. |
| output | comStatusOut | Fw.SuccessCondition | Passes ready status upstream. |
| output | encryptOut | Svc.Ccsds.CcsdsSdlsEncryption | Sends the SA index and data to the encryption helper. |
| sync input | encryptIn | Svc.Ccsds.CcsdsSdlsData | Receives the operation status and encrypted data from the helper. |
| output | encryptReturnOut | Svc.ComDataWithContext | Returns ownership of encrypted buffers to the helper. |
| sync input | bufferReturnIn | Svc.ComDataWithContext | Receives back the data buffer sent on `encryptOut`. |
| output | bufferAllocate | Fw.BufferGet | Allocates the frame buffer for the SA prepend. |
| output | bufferDeallocate | Fw.BufferSend | Deallocates frame buffers. |

Events: `EncryptionFailed` (WARNING_HI, carries the `SdlsStatus`) and `BufferAllocationFailed` (WARNING_HI, carries the requested size as `FwSizeType`).

Parameters: `SA_INDEX` (U16, default 0) — the SA index used when the incoming frame context does not specify one (context `saIndex` equal to its default value of 0xFFFF is treated as unset).

## Configuration

The `SA_INDEX` parameter selects the default security association for downlink frames.

## Unit Testing

Rule-based testing (STest) with rules covering both SA selection paths, both error paths, the encrypted-data framing path, the zero-copy in-place and fallback paths, the ownership return paths, and the comStatus pass-through; a 10000-step randomized scenario interleaves all rules. Requirements are traced with `REQUIREMENT()` macros in the test main.

## See Also

- [`Svc/Ccsds/Interfaces/CcsdsSdlsEncrypt.fpp`](../../Interfaces/CcsdsSdlsEncrypt.fpp)
- [`Svc/Ccsds/CcsdsSdlsDeframer`](../../CcsdsSdlsDeframer/docs/sdd.md)
- [`Svc/Ccsds/SdlsSaRouter`](../../SdlsSaRouter/docs/sdd.md)
