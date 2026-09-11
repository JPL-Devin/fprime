# Segmented deployment

Minimal test deployment that exercises the **TC Segment Header / MAP** uplink variants of the
communications subtopologies so that both are run through `fpp-to-cpp`, compiled and linked in CI
(`ref-segmented` job in `.github/workflows/ref.yml`). It is not a reference for flight use.

| `SEGMENTED_SDLS` | Topology file          | Communications subtopology            | Uplink path                                                                                     |
| ---------------- | ---------------------- | ------------------------------------- | ----------------------------------------------------------------------------------------------- |
| `OFF` (default)  | `Top/topology.fpp`     | `ComCcsds.SegmentedSubtopology`       | `frameAccumulator -> tcDeframerSeg -> tcMapReassembler -> spacePacketDeframer`                  |
| `ON`             | `Top/topologySdls.fpp` | `ComCcsdsSdls.SegmentedSubtopology`   | `frameAccumulator -> tcDeframerSeg -> sdlsDeframer/decryptor (SUCCESS) -> tcMapReassembler -> spacePacketDeframer` |

The SDLS variant overrides `ComCcsdsSdlsConfig.fpp` (`config/`) so that `ComCcsdsSdls.decryptor` is
`Svc.Ccsds.AesGcmDecryptor`, keyed by `sdlsKeyManager` from the checked-in **test** key
`test/int/sdls_test_key.bin` (32 octets `0x40..0x5F`; no security whatsoever). `Svc_Ccsds_AesGcmDecryptor`
is only registered when OpenSSL >= 3.5 is found; otherwise the SDLS configure fails (no clear-text fallback).

## Building

```bash
cd TestDeploymentsProject/SubtopologyBuilds/Segmented
# non-SDLS
fprime-util generate -DSEGMENTED_DEPLOYMENT=ON && fprime-util build -j"$(nproc)"
# SDLS (separate build cache; OpenSSL >= 3.5)
fprime-util generate --build-cache ../../build-seg-sdls -DSEGMENTED_DEPLOYMENT=ON -DSEGMENTED_SDLS=ON -DOPENSSL_ROOT_DIR=<openssl-3.5 prefix>
fprime-util build --build-cache ../../build-seg-sdls -j"$(nproc)"
```

`TestDeploymentsProject/CMakeLists.txt` is the only `project()`, so `Ref` and `Segmented` share the default
build cache and the same `build-artifacts/` install destination; the deployment is only added with
`-DSEGMENTED_DEPLOYMENT=ON` so that a plain `Ref` build installs a single deployment (GDS auto-detection). Use `--build-cache` to build several variants side by
side and, if the installed artifacts must not overwrite each other, `DESTDIR=<dir> fprime-util build ...` (the
`-DFPRIME_INSTALL_DEST` command-line option is overridden by `settings.ini`). The installed binary is
`<install>/<platform>/SubtopologyBuilds_Segmented/bin/SubtopologyBuilds_Segmented`.

## Running

```bash
./SubtopologyBuilds_Segmented -a 127.0.0.1 -p 50000                                   # non-SDLS
./SubtopologyBuilds_Segmented -a 127.0.0.1 -p 50000 --sdls-key-file test/int/sdls_test_key.bin   # SDLS (also -k <path>)
```

`Main.cpp` parses `-a`, `-p` and the long option `--sdls-key-file <path>` (`getopt_long`) into
`Segmented::TopologyState` (`SegmentedTopologyDefs.hpp`); the key file is required only by the SDLS build.
`test/int/int_config.json` maps the standard component roles for the GDS integration test API.

## Instances and base IDs

The deployment was written by hand (not with `fprime-util new --deployment`); it follows the Ref layout with the
`SegmentedTopologyCore.fppi` core shared by both variants.

| Instance                          | Type                           | Base ID         | Notes                                                  |
| --------------------------------- | ------------------------------ | --------------- | ------------------------------------------------------ |
| `rateGroup1Comp`                  | `Svc.ActiveRateGroup`          | `0x10001000`    | priority 43, queue 10, stack 64 KiB                    |
| `rateGroup2Comp`                  | `Svc.ActiveRateGroup`          | `0x10002000`    | priority 42                                            |
| `rateGroup3Comp`                  | `Svc.ActiveRateGroup`          | `0x10003000`    | priority 41                                            |
| `posixTime`                       | `Svc.PosixTime`                | `0x10020000`    |                                                        |
| `rateGroupDriverComp`             | `Svc.RateGroupDriver`          | `0x10021000`    | divisors 1, 2, 4 (`SegmentedTopology.cpp`)             |
| `linuxTimer`                      | `Svc.LinuxTimer`               | `0x10024000`    | 1 s period                                             |
| `comDriver`                       | `Drv.TcpClient`                | `0x10025000`    | `-a`/`-p`                                              |
| `sdlsKeyManager` (SDLS only)      | `Svc.Ccsds.SdlsFileKeyManager` | `0x10030000`    | `configure(state.sdlsKeyFile, 32)` in `configComponents` |
| `CdhCore.*`                       | `CdhCore.Subtopology`          | `CdhCoreConfig.BASE_ID`     | commands, events, telemetry, health, text logger |
| `FileHandling.*`                  | `FileHandling.Subtopology`     | `FileHandlingConfig.BASE_ID`| file uplink/downlink/manager, PrmDb              |
| `ComCcsds.*`                      | `ComCcsds.SegmentedSubtopology`| `ComCcsdsConfig.BASE_ID` (`0x02000000`) | incl. `tcDeframerSeg` `+0x0B000`, `tcPacketBufferManager` `+0x0C000`, `tcMapReassembler` `+0x0D000` |
| `ComCcsdsSdls.*` (SDLS only)      | `ComCcsdsSdls.SegmentedSubtopology` | `ComCcsdsSdlsConfig.BASE_ID` (`0x06000000`) | `decryptor` `+0x02000` (`AesGcmDecryptor`), `encryptor` `+0x04000` (`ClearTextEncryptor`) |

Rate group connections added on top of the Ref-style core (`SegmentedComStack*.fppi`):

| Port                                     | Connected to                                   |
| ---------------------------------------- | ---------------------------------------------- |
| `rateGroup1Comp.RateGroupMemberOut[3]`   | `<ComStack>.SegmentedSubtopology.comQueueRun`  |
| `rateGroup1Comp.RateGroupMemberOut[4]`   | `<ComStack>.SegmentedSubtopology.aggregatorTimeout` |
| `rateGroup3Comp.RateGroupMemberOut[2]`   | `<ComStack>.SegmentedSubtopology.bufferManagerSchedIn` |
| `rateGroup3Comp.RateGroupMemberOut[3]`   | `<ComStack>.SegmentedSubtopology.tcPacketBufferManagerSchedIn` |

SDLS-only connection (`Top/topologySdls.fpp`): `ComCcsdsSdls.decryptor.keyGet -> sdlsKeyManager.keyGet`.

## Dictionary discriminator for the SDLS fixture

`AesGcmDecryptor` defines no events, channels or commands, so it leaves no dictionary footprint. The SDLS
build is recognised in the generated dictionary by the **absence** of the event definition
`ComCcsdsSdls.decryptor.NullCipherInUse` (present when the default `ClearTextDecryptor` is selected) together
with the **presence** of `Segmented.sdlsKeyManager.KeyReadFailed`.
