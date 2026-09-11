# Svc::Ccsds::AesGcmDecryptor

The `Svc::Ccsds::AesGcmDecryptor` component decrypts and authenticates uplink data under AES-256-GCM, per CCSDS. It implements `Svc.Ccsds.CcsdsSdlsDecrypt` toward a decryption client (typically [`Svc::Ccsds::CcsdsSdlsDeframer`](../../CcsdsSdlsDeframer/docs/sdd.md) via [`Svc::Ccsds::SdlsSaRouter`](../../SdlsSaRouter/docs/sdd.md)) and `Svc.Ccsds.SdlsKeyInterfaceClient` toward a key supplier such as [`Svc::Ccsds::SdlsFileKeyManager`](../../SdlsFileKeyManager/docs/sdd.md). It is the security-providing alternative to [`Svc::Ccsds::ClearTextDecryptor`](../../ClearTextDecryptor/docs/sdd.md), and the uplink mirror of [`Svc::Ccsds::AesGcmEncryptor`](../../AesGcmEncryptor/docs/sdd.md).

## Introduction

Decrypting a frame proceeds as follows:

1. Receive an SA index, ciphertext buffer (`IV (12) | ciphertext | MAC (16)`), and frame context on `decryptIn`.
2. Reject a buffer too short to hold an IV and a MAC with `DECRYPTION_FAILURE` — before requesting a key, since the uplink is attacker-influenced.
3. Request the AES-256 key for the frame's security association via `keyGet`; on failure or a wrong-sized key, report `KEY_ERROR`.
4. Decrypt in place, authenticating the SA index, the virtual channel and, when the frame carried one, the TC Segment Header as AES-GCM additional authenticated data (AAD).
5. On a failed MAC check, report `MAC_VERIFICATION_FAILURE`, distinct from `DECRYPTION_FAILURE`; the component remains able to decrypt subsequent frames.
6. On success, narrow the buffer to the plaintext and emit it on `decryptOut` with status `SUCCESS`.

Decryption is in place, so the emitted buffer is the one received, advanced past the IV and narrowed to the plaintext length; `Fw::Buffer` keeps its allocation context independently of the data pointer, so it remains deallocatable by the issuing `Svc.BufferManager`. Buffers returned on `decryptReturnIn` are passed upstream via `bufferReturnOut` unconditionally, since every buffer emitted is the one that arrived.

The AAD is built by `Svc::Ccsds::Utils::SdlsTcAuthMask`, whose layout matches the ground segment's independent implementation of the same contract. The virtual channel and the Segment Header come from the frame context: `Svc::Ccsds::TcDeframer` reads the VCID from the TC primary header and, in Segment Header mode, the Segment Header octet from the first octet of the frame data field, and records both on the context (`vcId`, `tcSegmentHeaderPresent`, `tcSegmentHeader`) before stripping those fields, so they are still available by decryption time.

## Security Considerations

What this component provides: confidentiality, integrity, and authentication of the frame body, bound to the SA index, the virtual channel and, when present, the TC Segment Header. A frame modified in flight, built under a different key, presented on a different virtual channel or SA, or whose Segment Header (sequence flags or MAP ID) differs from the one the sender authenticated fails the MAC check. This is what lets a downstream `Svc::Ccsds::TcMapReassembler` trust the Segment Header it receives: it is wired after the `Svc::Ccsds::CcsdsSdlsDeframer` `SUCCESS` gate, and that gate authenticates only when the connected decryptor verifies the MAC — this component does; the default `Svc::Ccsds::ClearTextDecryptor` returns `SUCCESS` for every frame and provides no protection against forged Segment Headers.

What it does not provide, and which an operator must plan for:

- **No anti-replay.** The IV is chosen by the sender and is not checked against anything, and no sequence number is tracked per SA. A previously valid TC frame captured off the link and re-injected later passes the MAC check and reaches the command path unchanged. SDLS anti-replay (CCSDS 355.0-B-2 §4.1.3) relies on an Anti-Replay Sequence Number in the security header, which this implementation does not carry. Authenticating the Segment Header does not change this: a replayed authentic `FIRST` or `UNSEGMENTED` frame is accepted and, downstream, abandons any packet in progress on its MAP. Missions exposed to a recording adversary need replay protection above this layer — a command counter, a time-bounded authorization window, or an idempotent command set.
- **No key rotation.** The key is fetched per frame from the connected key source, so rotation is that component's responsibility. The in-tree `Svc.Ccsds.SdlsFileKeyManager` serves one static key for the life of the process.
- **Fixed authentication mask.** CCSDS 355.0-B-2 §2.3.2.2 makes the authentication bit mask a managed parameter per SA; this implementation uses one compile-time mask (VCID bits and SPI verbatim, Segment Header verbatim when present, everything else zero) for every SA. A ground segment configured with a different mask does not interoperate.

## Requirements

| Name | Description | Rationale | Validation |
|---|---|---|---|
| SVC-CCSDS-AES-DECRYPTOR-001 | On a successful request, the AesGcmDecryptor shall decrypt the buffer in place and emit the plaintext on `decryptOut` with status `SUCCESS`. | Plaintext is never longer than ciphertext, so no second buffer is needed. | Unit Test |
| SVC-CCSDS-AES-DECRYPTOR-002 | The AesGcmDecryptor shall authenticate, as AES-GCM AAD, the masked TC primary header carrying the virtual channel ID together with the SA index, and reject a frame authenticated for any other VC or SA. | CCSDS 355.0-B-2 binds a frame to its VC and SA. | Unit Test |
| SVC-CCSDS-AES-DECRYPTOR-003 | The AesGcmDecryptor shall return `MAC_VERIFICATION_FAILURE`, distinct from `DECRYPTION_FAILURE`, when the IV, ciphertext, or MAC has been modified or the frame was not built for this SA and VC, and shall remain able to decrypt subsequent frames. | CCSDS 355.0-B-2 sect. 3.3.3.2 requires an authentication verdict distinguishable from a processing error; the two call for different ground responses. | Unit Test |
| SVC-CCSDS-AES-DECRYPTOR-004 | The AesGcmDecryptor shall return `DECRYPTION_FAILURE`, without requesting a key, for a buffer too short to hold an IV and a MAC. | An uplink buffer is attacker-influenced and must be bounded before use; the ciphertext length is handed to OpenSSL as an `int`. | Unit Test |
| SVC-CCSDS-AES-DECRYPTOR-005 | The AesGcmDecryptor shall return `KEY_ERROR` when the key manager reports failure or supplies a key that is not AES-256 sized. | A wrong-sized key would otherwise decrypt under unintended material. | Unit Test |
| SVC-CCSDS-AES-DECRYPTOR-006 | The buffer emitted on `decryptOut` shall retain the allocation context and original allocation pointer of the buffer received on `decryptIn`. | `Svc.BufferManager` deallocates by context and asserts the data pointer lies within the slot it issued. | Unit Test |
| SVC-CCSDS-AES-DECRYPTOR-007 | A buffer received on `decryptReturnIn` shall be returned on `bufferReturnOut`. | Every buffer emitted is the one that arrived. | Unit Test |
| SVC-CCSDS-AES-DECRYPTOR-008 | When the frame context reports a TC Segment Header (`tcSegmentHeaderPresent`), the AesGcmDecryptor shall include the received Segment Header octet in the AAD between the masked primary header and the SA index, build the AAD from the context of each frame, and pass the resulting 19- or 20-octet length to the cipher; a frame whose Segment Header differs from the one authenticated shall fail the MAC check. | CCSDS 355.0-B-2 §2.3.2.2 authenticates the Segment Header; the sequence flags change from frame to frame, and a forged Segment Header must not reach the MAP reassembler. | Unit Test |

## Design

The component is passive with no commands, telemetry, or parameters, and allocates no memory after construction. It composes two interfaces:

| Kind | Name | Port Type | Description |
|---|---|---|---|
| guarded input | decryptIn | Svc.Ccsds.CcsdsSdlsEncryption | Receives the SA index and ciphertext buffer to decrypt. |
| output | decryptOut | Svc.Ccsds.CcsdsSdlsData | Sends the operation status and decrypted data upstream. |
| sync input | decryptReturnIn | Svc.ComDataWithContext | Receives back ownership of buffers sent on `decryptOut`. |
| output | bufferReturnOut | Svc.ComDataWithContext | Returns the incoming buffer for deallocation. |
| output | keyGet | Svc.Ccsds.SdlsKey | Requests the AES-256 key bound to the frame's security association. |

The component emits no events: every outcome, including a failed authentication, is reported as an `SdlsStatus` on `decryptOut`.

### Additional authenticated data

The AAD is built for every frame, on the stack, from the SA index and the frame context, as `Svc::Ccsds::Utils::SdlsTcAuthMask(vcId, saIndex, tcSegmentHeaderPresent, tcSegmentHeader)`. No mask is cached between frames: the Segment Header sequence flags differ between consecutive frames of one packet, so a cache keyed on VC and SA would authenticate a frame against a stale mask. The mask's runtime `size` — not the size of its 20-octet backing array — is what reaches `EVP_DecryptUpdate`.

| Offset | No Segment Header (19 octets) | Segment Header present (20 octets) |
|---|---|---|
| 0..1 | `00 00` (version, bypass, control, spare, SCID masked) | `00 00` |
| 2 | `vcId << 2` (VCID bits 7..2; frame length bits masked) | `vcId << 2` |
| 3..4 | `00 00` (frame length, sequence number masked) | `00 00` |
| 5 | SPI high octet | received Segment Header octet, verbatim |
| 6 | SPI low octet | SPI high octet |
| 7 | `00` (IV field zeroed) | SPI low octet |
| 8..18 | `00` … | `00` … (IV field zeroed) |
| 19 | — | `00` |

The Segment Header is copied into the AAD exactly as received, so a frame is accepted only if the ground segment authenticated the same octet; a frame carrying a Segment Header that is presented with `tcSegmentHeaderPresent = false` fails, as does a frame without one presented as if it had one.

## Configuration

Compile time: none.

Runtime: none. The constructor builds the `EVP_CIPHER_CTX` every frame reuses, which is what keeps `decryptIn` free of dynamic allocation, and the authenticated virtual channel arrives per frame on the frame context. The [`Svc/Ccsds/AesGcmEncryptor`](../../AesGcmEncryptor/docs/sdd.md) downlink path is built the same way.

A key source must be connected to `keyGet` and made ready before the first frame arrives; the component requests a key per frame and reports `KEY_ERROR` if none is available. The in-tree `Svc.Ccsds.SdlsFileKeyManager` needs `configure(path, keySize)` during topology setup.

## Unit Testing

Direct tests covering in-place decryption and its allocation context, the short-buffer shape-check rejection, both key-error paths, and the buffer return path. Authentication is exercised by tampering with the IV, ciphertext, and MAC in turn, and by presenting frames built for another VC and another SA, with a following good frame confirming the shared cipher context survives a rejection. The AAD and a known-answer frame are checked against an independently built reference rather than assumed self-consistent. Requirements are traced with `REQUIREMENT()` macros in the test main.

The Segment Header path is checked with known-answer vectors generated by an independent implementation (python `cryptography` AES-256-GCM), all under the same key, IV, plaintext and ciphertext as the 19-octet vector:

| Vector | VC | SPI | Segment Header | AAD | Test |
|---|---|---|---|---|---|
| Baseline (unchanged) | 5 | 0x1234 | none | 19 octets | `KnownAnswer`, `AadLengthPassed` |
| FIRST | 5 | 0x1234 | `0x41` | 20 octets | `DecryptVectorFirst`, `ShNotInAadFails`, `ShTamperFails`, `AadLengthPassed` |
| UNSEGMENTED | 5 | 0x1234 | `0xC1` | 20 octets | `DecryptVectorUnsegmented` |
| CONTINUING, LAST | 5 | 0x1234 | `0x01`, `0x81` | 20 octets | `DecryptVectorContinuingLast` |
| End-to-end S1, S2 | 1 | 0x0001 | `0x40`, `0x80` | 20 octets | `DecryptFrameVc1` (S1 also fails under a VC 0 context) |
| End-to-end S2 with last MAC octet flipped | 1 | 0x0001 | `0x80` | 20 octets | `TamperedMacVector` |
| End-to-end UNSEGMENTED | 0 | 0x0001 | `0xC0` | 20 octets | `DecryptFrameVc0` |

`AuthMaskNoSh` and `AuthMaskSh` check the 19- and 20-octet layouts of `SdlsTcAuthMask` directly, and `PerFrameAad` alternates frames with and without a Segment Header through one component instance.

## See Also

- [`Svc/Ccsds/Interfaces/CcsdsSdlsDecrypt.fpp`](../../Interfaces/CcsdsSdlsDecrypt.fpp)
- [`Svc/Ccsds/Interfaces/SdlsKey.fpp`](../../Interfaces/SdlsKey.fpp)
- [`Svc/Ccsds/AesGcmEncryptor`](../../AesGcmEncryptor/docs/sdd.md)
- [`Svc/Ccsds/CcsdsSdlsDeframer`](../../CcsdsSdlsDeframer/docs/sdd.md)
- [`Svc/Ccsds/SdlsSaRouter`](../../SdlsSaRouter/docs/sdd.md)
