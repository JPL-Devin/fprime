# ComCcsdsSdls (CCSDS Framing with SDLS Decryption) Subtopology — Software Design Document (SDD)

The **ComCcsdsSdls subtopologies** implement F´'s **CCSDS** communications stack for framing/deframing on the flight side, with an **SDLS (Space Data Link Security) decryption stage** inserted in the uplink path. As with `ComCcsds`, there are **two variants** in the same module:

1. A variant that **supplies a `Svc::ComStub`** implementation of `Svc.ComInterface` and expects to be wired to a **`Drv::ByteStreamDriverModel`** (TCP/UDP/UART, etc.), and
2. A variant that **expects an external implementation of [`Svc.ComInterface`](https://fprime.jpl.nasa.gov/latest/docs/reference/communication-adapter-interface/)** provided by the deployment.

Both variants provide the standard **router + ComQueue + CCSDS framers/deframers** path, plus an **SDLS decryption chain** (`CcsdsSdlsDeframer` → `SdlsSaRouter` → decryptor), tuned through **ComCcsdsSdlsConfig** instance properties.

> [!WARNING]
> The **default decryptor is `Svc.Ccsds.ClearTextDecryptor`, which provides NO security** — no confidentiality, no integrity, and no authentication. Projects requiring security must override the configuration module to select a real decryptor implementation.

---

## 1. Requirements

| ID                   | Description                                                                                                                                              | Validation |
| -------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------- |
| SVC-COMCCSDSSDLS-001 | The subtopology shall provide the standard CCSDS framing/deframing communications stack (router, ComQueue, framers/deframers) with SDLS decryption inserted in the uplink path. | Inspection |
| SVC-COMCCSDSSDLS-002 | The uplink path shall pass TC-deframed data through a `Svc.Ccsds.CcsdsSdlsDeframer`, which extracts the SA index and delegates decryption before Space Packet deframing. | Inspection |
| SVC-COMCCSDSSDLS-003 | Decryption requests shall be routed by SA index through a `Svc.Ccsds.SdlsSaRouter` to downstream decryptor instances.                                    | Inspection |
| SVC-COMCCSDSSDLS-004 | The decryptor choice shall be configurable via the subtopology configuration module, defaulting to `Svc.Ccsds.ClearTextDecryptor`.                       | Inspection |
| SVC-COMCCSDSSDLS-005 | The default SA map shall route SA 0 to the `PLAINTEXT_DECRYPTION` port (the default decryptor); remaining default entries route to ports left unconnected. | Inspection |
| SVC-COMCCSDSSDLS-006 | The module shall provide a `FramingSubtopology` (external `Svc.ComInterface`) and a `Subtopology` (supplies `Svc::ComStub`) variant, mirroring ComCcsds. | Inspection |
| SVC-COMCCSDSSDLS-007 | Instance properties (base ID, queue/stack sizes, priorities, buffer sizing) shall be configurable via a `ComCcsdsSdlsConfig` module.                     | Inspection |

---

## 2. Design & Core Functions

### 2.1 Instance Summary

| Instance name         | Type (Svc)                      | Kind    | Purpose (core function)                                                                          |
| --------------------- | ------------------------------- | ------- | ------------------------------------------------------------------------------------------------ |
| `fprimeRouter`        | `Svc.FprimeRouter`              | Passive | Routes deframed packets (e.g., commands/files) into the flight software.                         |
| `comQueue`            | `Svc.ComQueue`                  | Active  | Queues categorized COM data for framing (telemetry, events, file, etc.); exposes `run`.          |
| `spacePacketFramer`   | `Svc.Ccsds.SpacePacketFramer`   | Passive | Builds **CCSDS Space Packets** from COM buffers (downlink step 1).                               |
| `framer`              | `Svc.Ccsds.TmFramer`            | Passive | Builds **CCSDS TM Transfer Frames** from space packets and sends to the link (downlink step 2).  |
| `frameAccumulator`    | `Svc.FrameAccumulator`          | Passive | Collects bytes from the link and emits complete frames for deframing (uplink step 1).            |
| `tcDeframer`          | `Svc.Ccsds.TcDeframer`          | Passive | Deframes **CCSDS TC Transfer Frames** (uplink step 2).                                           |
| `sdlsDeframer`        | `Svc.Ccsds.CcsdsSdlsDeframer`   | Passive | Extracts the SA index from the SDLS frame and delegates decryption (uplink step 3).              |
| `saRouter`            | `Svc.Ccsds.SdlsSaRouter`        | Passive | Routes decryption requests by SA index to the mapped downstream decryptor.                       |
| `decryptor`           | `Svc.Ccsds.ClearTextDecryptor`* | Passive | Default decryptor for the base SA (**pass-through, NO security**). *Configurable — see 2.3.      |
| `spacePacketDeframer` | `Svc.Ccsds.SpacePacketDeframer` | Passive | Deframes F Prime data from **CCSDS Space Packets** (uplink step 4).                              |
| `comStub`             | `Svc.ComStub`                   | Passive | (Variant A only) Implementation of `Svc.ComInterface`, adapting a `Drv::ByteStreamDriverModel`.  |

> **Two variants:**
> **A. "With ComStub" (`Subtopology`):** includes `Svc::ComStub` and exposes **ByteStream** ports to your driver.
> **B. "With External ComInterface" (`FramingSubtopology`):** you **provide** an `Svc.ComInterface` implementation in the deployment.

### 2.2 Uplink Data Flow (with SDLS)

```
frameAccumulator -> tcDeframer -> sdlsDeframer -> spacePacketDeframer -> fprimeRouter
                                       |  ^
                            decryptOut v  | decryptIn (decrypted data)
                                    saRouter
                                       |  ^
                       saDecryptOut[0] v  | saDecryptIn[0]
                                    decryptor
```

The `sdlsDeframer` extracts the leading 16-bit SA index, records it in the frame context, and sends the remaining iv/data to the `saRouter`, which maps the SA to the decryptor on the mapped port. Decrypted data flows back through the router and deframer to the `spacePacketDeframer`. Buffer ownership returns flow the reverse paths (`dataReturnIn` → `decryptReturnOut` → decryptor; decryptor `bufferReturnOut` → router `bufferReturnOut` → deframer `dataReturnOut`).

### 2.3 Selecting a Different Decryptor

The `decryptor` instance is defined in the configuration module (`ComCcsdsSdlsConfig/ComCcsdsSdlsConfig.fpp`), not in the subtopology itself. Projects override the configuration module (CMake `CONFIGURATION_OVERRIDES`) to instantiate a different component implementing the `Svc.Ccsds.CcsdsSdlsDecrypt` interface. To route additional SAs to additional decryptors, also override the `SdlsSaRouter` configuration (`SdlsCfg.SaMap`, `SdlsCfg.SaRouterPortCount`) and connect the added router ports in the deployment topology.

### 2.4 Default SA Map

The `SdlsSaRouter` default configuration is two deep: `{ SA 0 -> SaRouterPorts.PLAINTEXT_DECRYPTION, SA 1 -> SaRouterPorts.UNCONNECTED }`. The subtopology connects only the `PLAINTEXT_DECRYPTION` port (the default decryptor); the `UNCONNECTED` port is left unconnected, so its SA returns `UNKNOWN_PORT` unless a deployment connects an additional decryptor. The SA mapping is configurable by overriding the `SdlsSaRouter` configuration module.

### 2.5 Required Inputs for Operation

* **Rate Groups:** Connect a rate group to **`comQueue.run`** (telemetry send rate) and **`aggregator.timeout`**.
* **Transport Endpoint:** wire the ComStub ByteStream ports (variant A) or an external `Svc.ComInterface` (variant B) as documented in the usage note in `ComCcsdsSdls.fpp`.

## 3. Configuration

`ComCcsdsSdlsConfig` mirrors `ComCcsdsConfig`: `BASE_ID`, `QueueSizes`, `StackSizes`, `Priorities`, `QueueDepths`, `QueuePriorities`, and `BuffMgr` constants, plus the `decryptor` instance definition (see 2.3). The memory allocator is supplied via `ComCcsdsSdls::Allocation::memAllocator` in `ComCcsdsSdlsSubtopologyConfig.cpp`.

## 4. See Also

- [ComCcsds subtopology](../../ComCcsds/docs/sdd.md)
- [`Svc::Ccsds::CcsdsSdlsDeframer`](../../../Ccsds/CcsdsSdlsDeframer/docs/sdd.md)
- [`Svc::Ccsds::SdlsSaRouter`](../../../Ccsds/SdlsSaRouter/docs/sdd.md)
- [`Svc::Ccsds::ClearTextDecryptor`](../../../Ccsds/ClearTextDecryptor/docs/sdd.md)
