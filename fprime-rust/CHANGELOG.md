# Changelog

All notable changes to `fprime-rust` will be documented in this file.

This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - Unreleased

### Added

- Initial public scaffolding for a Rust autocoder for F Prime.
- `fprime-rust-ac` CLI with `bindings` and `crate` subcommands.
- C++ shim generator (`<Component>RustImpl.{hpp,cpp}`).
- Rust base/template generator (`<component>_base.rs`, `<Component>.template.rs`).
- Cargo workspace generator (`Cargo.toml`, `lib.rs`, `rust-toolchain.toml`).
- `fprime-rust` runtime crate exposing `CmdResponse`, `ParamValid`, and
  `Context`.
- CMake autocoder hook (`cmake/autocoder/fprime_rust.cmake`).
- CMake target hook (`cmake/target/fprime_rust.cmake`).
- `Ref/RustExample/` reference component demonstrating a command, an event,
  a telemetry channel, and a parameter.
- Software design description (`docs/design.md`) and architecture overview
  (`docs/architecture.md`).
- `pytest` test suite covering the FPP parser, the generators, and the CLI.
