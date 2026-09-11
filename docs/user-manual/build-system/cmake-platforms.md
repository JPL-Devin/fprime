# F´ and CMake Platforms

Users can create platform-specific build files for the purposes of tailoring fprime
for given platform targets. Any CMake toolchain file should work, but it will require a platform file created here to add target-specific configuration using the name "${FPRIME_PLATFORM}.cmake".

Platforms should register a configuration module using `register_fprime_config` that sets the `AUTOCODER_INPUTS`, `HEADERS`,
`CHOOSES_IMPLEMENTATIONS`, and `BASE_CONFIG` directives.

`AUTOCODER_INPUTS`: must include one .fpp file defining the platform's [platform types](../../reference/numerical-types.md#platform-configured-types)
`HEADERS`: lists the `PlatformTypes.h` header defining `PlatformPointerCastType`
`CHOOSES_IMPLEMENTATIONS`: lists all implementations chosen for the current platform. See: [CMake Implementations](./cmake-implementations.md).
`BASE_CONFIG`: makes the platform types visible to every module without an explicit dependency.

The platform's configuration directory must not sit directly under an include root, and platform-wide compile
definitions (e.g. `-DTGT_OS_TYPE_LINUX`) are attached to the configuration target with `target_compile_definitions`.
See `cmake/platform/Linux.cmake` and `cmake/platform/unix/Platform/CMakeLists.txt` for the reference implementation, and
[Configuration Modules](./configuration.md) for how configuration modules are assembled and overridden.
