# fprime-rust Software Design Description

> **Document scope.**  This SDD describes the `fprime-rust` autocoder, its
> supporting CMake integration, the Rust runtime crate, and the contract it
> exposes to F Prime component authors.  It follows the structure of the
> [`fprime-python` design](https://github.com/fprime-community/fprime-python)
> with adaptations for the FFI-based Rust integration.

## 1. Purpose

`fprime-rust` extends F Prime so that component bodies can be written in
**Rust** instead of C++ (or Python).  The goals are:

- **Modeling parity** with C++ and Python: a Rust component is still defined
  by an FPP component model, still uses the standard F Prime topology, and
  still produces dictionaries / SBOM / installer artifacts identical to a
  C++ component of the same shape.
- **Zero changes to the F Prime core**: integration is via the existing
  `library_locations` / `register_fprime_target` extension points.
- **Predictable build**: the Rust crate is built with stock `cargo` and its
  ABI surface is restricted to a small `extern "C"` FFI so that the
  cross-language link is reproducible across compilers.

## 2. Stakeholders

| Role                | Concern                                                                                |
|---------------------|----------------------------------------------------------------------------------------|
| Flight SW developer | Authoring components in Rust against an FPP model.                                     |
| Build/CI engineer   | Reproducibility, hermetic Cargo build, CMake/Ninja integration.                        |
| F Prime maintainer  | Out-of-tree library; opt-in via annotation; no patches to existing F Prime files.      |
| Mission integrator  | Standard F Prime artifacts (dictionary, telemetry packets, command opcodes) preserved. |

## 3. Architectural overview

```
┌──────────────────────────┐      ┌──────────────────────────┐
│  FPP file (annotated)    │      │  fprime-util / CMake     │
│                          │      │                          │
│  @ fprime-rust           │      │   generate / build / ut  │
│  active component Foo {} │      │                          │
└────────────┬─────────────┘      └────────────┬─────────────┘
             │                                 │
             ▼                                 ▼
┌──────────────────────────┐      ┌──────────────────────────────────┐
│   FPP core autocoders    │      │   fprime-rust-ac (this package)  │
│   (fpp-to-cpp, etc.)     │      │                                  │
│                          │      │   bindings / crate subcommands   │
│   FooComponentBase.{h,c} │      │                                  │
└────────────┬─────────────┘      └────────────┬─────────────────────┘
             │                                 │
             ▼                                 ▼
┌─────────────────────────────────────────────────────────────────────┐
│ Build cache                                                         │
│                                                                     │
│   FooRustImpl.hpp           <──── derives from FooComponentBase     │
│   FooRustImpl.cpp           <──── extern "C" FFI to libfprime_rust  │
│   foo_base.rs                                                       │
│   Foo.template.rs ──────────> source dir (one-time copy)            │
│   _fprime_rust_crate/foo_crate/{Cargo.toml, lib.rs, rust-toolchain} │
│                                                                     │
│              cargo build --release  ─►  libfprime_rust_foo.a        │
└────────────┬────────────────────────────────┬───────────────────────┘
             │                                │
             └────────────────►  ld  ◄────────┘
                                  │
                                  ▼
                             Deployment ELF
```

## 4. Major components

### 4.1 `fprime-rust-ac` (Python autocoder)

- Package: `fprime_rust.autocode` (entry point in `__main__.py`).
- Subcommands:
  - `bindings` -- emit `<Component>RustImpl.{hpp,cpp}`,
    `<component>_base.rs`, `<Component>.template.rs`.  A `--dry-run`
    discovery mode is supported so CMake can declare the file set up front.
  - `crate` -- emit `Cargo.toml`, `lib.rs`, `rust-toolchain.toml` for each
    annotated component in a module.
- FPP discovery is performed by [`fpp_parser.py`](../src/fprime_rust/autocode/fpp_parser.py),
  a regex-driven parser that identifies module nesting, `@ fprime-rust` marks,
  and the four supported member kinds (commands, events, telemetry,
  parameters) with their primitive arguments.  We consciously side-step the
  full FPP AST for the MVP because (a) it avoids a dependency on
  `fprime-python-model` (not on PyPI), and (b) the autocoder's job is small
  enough that grammar fidelity beyond annotated component bodies is not
  required.  The full AST is still consumed downstream by `fpp-to-cpp` for
  the *base* component generation.

### 4.2 C++ shim

The shim is a `final` subclass of the FPP-generated base that:

1. Allocates a `Box<dyn Component>` on construction via the Rust-exported
   `rust_<comp>_new` constructor.  The opaque `void*` is held in
   `m_rust_impl` for the lifetime of the C++ object.
2. Overrides every `*_cmdHandler` defined in the FPP model.  The override
   forwards arguments verbatim to a Rust function declared
   `extern "C" rust_<comp>_cmd_<name>(...)` with the same primitive types.
3. Exports a static set of "sink" functions (in the global namespace, with
   stable C linkage) that Rust calls back into to publish telemetry, log
   events, fetch parameters, and respond to commands.  Sinks take a
   `void* fp_self` first argument and `static_cast` it back to the C++
   instance pointer.

### 4.3 Rust base module

The generated `<component>_base.rs` (regenerated every build) declares:

- A `Component` trait with one method per command.  The first argument is
  always `&mut self` (so user state is isolated by component instance) and
  the second is `ctx: &mut Context` (so user code can publish telemetry /
  events / cmd response back to F Prime).
- An `extern "C"` block describing the C++ sink functions.  This is
  intentionally separate from the inbound dispatch so that the unsafe
  surface can be audited in one place.
- `#[no_mangle] pub unsafe extern "C"` shims for inbound dispatch.  These
  reconstruct the `Box<dyn Component>` from the opaque pointer, build a
  `Context`, and call into the user trait.  The shims uphold the
  contract that `cpp_self` is non-null (verified at runtime) and that
  `Box::into_raw` is matched by exactly one `Box::from_raw`.

### 4.4 User template

The user template (`<Component>.template.rs`) is seeded **once** into the
source directory (CMake post-build step skips the copy if a hand-written
`<Component>.rs` already exists).  This mirrors `fprime-python`'s
"copy once" template behavior and prevents accidental clobbering of user
edits on subsequent builds.

### 4.5 `fprime-rust` runtime crate

A small Rust crate (`runtime/`) with three exports:

- `CmdResponse` -- `repr(u8)` enum of the F Prime command response codes.
- `ParamValid` -- `repr(u8)` enum of the F Prime parameter validity codes
  with a `from_raw(u8)` decoder that fails closed.
- `Context` -- a `Send` (not `Sync`) struct holding the opaque C++ pointer
  and the in-flight command identifiers.  All other helper methods (e.g.
  `tlm_write_Foo`, `log_Bar`, `param_Baz`) are *generated per component* so
  they can match the FPP-declared types exactly.

### 4.6 CMake integration

Two CMake files:

- `cmake/autocoder/fprime_rust.cmake` -- registers `fprime_rust` as a
  per-module autocoder.  The autocoder accepts every `.fpp` file and lets
  the Python tool decide which components are annotated.
- `cmake/target/fprime_rust.cmake` -- runs the autocoder for a module,
  materialises the per-component Cargo workspace, and adds custom commands
  to invoke `cargo build --release`.  The resulting `.a` is fed back into
  the F Prime module's link line via `target_link_libraries`.

The integration deliberately mirrors `target/fprime_python.cmake` so users
familiar with the Python flow will recognise the structure.

## 5. Threading & memory model

- **Ownership.**  The Rust impl is owned by the C++ component instance.
  The C++ destructor calls `rust_<comp>_free`, which `Box::from_raw`s and
  drops the impl.  No reference counting or shared ownership.
- **Concurrency.**  F Prime's executor serialises handler calls per
  component instance, so handler trait methods take `&mut self`.
  `Context` is `Send` but not `Sync`; user code must not stash it across
  threads.
- **Aliasing.**  `Context::cpp_self` is a raw pointer; the runtime never
  dereferences it from Rust.  All access to F Prime state is via the
  generated FFI sinks, which take the pointer back into C++.

## 6. Error handling

- All FFI sinks return either `void` or a primitive value; errors are
  surfaced through the existing F Prime channels (event log, command
  response).  The Rust runtime never panics across the FFI boundary --
  user code is expected to either handle errors locally or report them
  via `ctx.log_*` / `ctx.command_response(CmdResponse::ExecutionError)`.
- The Cargo build is invoked with `--release`.  `cargo` failures fail the
  CMake target.

## 7. Build & integration

The build pipeline for a Rust-backed module is:

1. `fpp-to-cpp` generates `<Component>ComponentBase.{hpp,cpp}` (unchanged).
2. `fprime-rust-ac bindings --dry-run` produces the file list.
3. `fprime-rust-ac bindings` (real run) writes the shim + base + template.
4. `fprime-rust-ac crate` lays down the per-component Cargo workspace.
5. `cargo build --release` produces `libfprime_rust_<crate>.a`.
6. CMake links the staticlib into the C++ module and the deployment.

## 8. Testing strategy

- **Autocoder unit tests** (Python).  Each generator (`fpp_parser`,
  `cpp_generator`, `rust_generator`, `cargo_generator`) has a fixture-based
  test that runs against the sample `RustExample.fpp` and asserts on the
  rendered output.  See `tests/`.
- **Generated Rust crate compiles**.  CI is expected to invoke
  `cargo check` against `runtime/` and against the generated user crates
  (the latter requires a full deployment build).
- **End-to-end deployment tests** (future).  When integrated with the F
  Prime test harness, the Rust impl is exercised via the standard
  command/telemetry round-trip used for C++ components.

## 9. Risks & mitigations

| Risk                                                        | Mitigation                                                                           |
|-------------------------------------------------------------|--------------------------------------------------------------------------------------|
| FPP grammar drift breaks the regex parser.                  | Parser is restricted to component bodies; full FPP type checking still runs in core. |
| Cross-language link surface grows untracked.                | All FFI symbols are auto-generated; a single ``extern "C"`` block per component.     |
| Cargo toolchain availability in flight CI.                  | `rust-toolchain.toml` pins the channel; CMake gracefully degrades when cargo missing.|
| User edits the generated base file.                         | File header banner reads "do not edit"; CMake always overwrites on build.            |
| Multiple Rust components in a module.                       | One Cargo crate per component; per-crate `.a` linked individually.                   |

## 10. Future work

- Richer FPP type support (arrays, structs, enums, strings).
- Component unit-test harness (mirror of `Tester` C++ class).
- `fprime-bootstrap` integration for new Rust-first projects.
- Cross-compilation playbook for embedded / RTOS targets.
