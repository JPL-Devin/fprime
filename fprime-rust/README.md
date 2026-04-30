# fprime-rust: F´ to Rust Bindings

`fprime-rust` is an autocoder for [F Prime](https://fprime.jpl.nasa.gov/) that
lets you implement components in **Rust** while reusing the standard FPP model,
component bases, and topology infrastructure.  It is intentionally modeled
after the existing
[`fprime-python`](https://github.com/fprime-community/fprime-python) project so
that operators familiar with one feel at home in the other.

> **Status:** Experimental.  The MVP supports primitive-typed commands, events,
> telemetry, and parameters.  Richer FPP types (arrays, structs, enums) can
> be added without API churn -- see
> [`docs/design.md`](docs/design.md) for the extension surface.

## Why?

- Lets project teams put performance- or safety-critical logic in Rust without
  abandoning the F Prime build/model/topology workflow.
- Keeps the FPP model authoritative -- you write FPP, you implement in Rust,
  the autocoder regenerates the binding code on every build.
- Reuses the C++ component base verbatim, so existing F Prime tooling
  (Dictionary, GDS, SBOM, install rules) keeps working.

## How it works

Component authors mark an FPP component with `@ fprime-rust`:

```fpp
module Ref {
    @ Example component implemented in Rust
    @ fprime-rust
    queued component RustExample {
        # ... commands, events, telemetry, parameters ...
    }
}
```

`fprime-rust-ac` (run automatically by CMake) generates four files for the
component:

| File                                       | Role                                                |
|--------------------------------------------|-----------------------------------------------------|
| `<Component>RustImpl.hpp`                  | C++ shim header (final subclass of FPP base).       |
| `<Component>RustImpl.cpp`                  | C++ shim source -- forwards handlers to Rust.       |
| `<component>_base.rs`                      | Rust base module -- trait, FFI, dispatch shims.     |
| `<Component>.template.rs` -> `<Component>.rs` | One-time user template seeded into the source dir. |

You then implement behavior in `<Component>.rs` by `impl`-ing the auto-coded
trait.  Example:

```rust
use fprime_rust::CmdResponse;
use crate::rust_example_base::{Component, Context};

pub struct Implementation { /* state */ }

impl Implementation {
    pub fn new() -> Self { Self { /* ... */ } }
}

impl Component for Implementation {
    fn Reset_cmd_handler(&mut self, ctx: &mut Context) {
        ctx.tlm_write_Counter(0);
        ctx.log_ResetIssued();
        ctx.command_response(CmdResponse::Ok);
    }
}
```

## Repository layout

```
fprime-rust/
├── README.md                      # this file
├── library.cmake                  # F Prime library entry point
├── pyproject.toml                 # Python autocoder package metadata
├── cmake/
│   ├── autocoder/fprime_rust.cmake   # autocoder hook
│   └── target/fprime_rust.cmake      # module/deployment target hook
├── src/fprime_rust/                  # autocoder Python source
│   └── autocode/
│       ├── __main__.py               # `fprime-rust-ac` CLI
│       ├── fpp_parser.py             # FPP -> dataclasses
│       ├── cpp_generator.py          # C++ shim emitter
│       ├── rust_generator.py         # Rust trait/FFI emitter
│       └── cargo_generator.py        # Cargo workspace emitter
├── runtime/                          # `fprime-rust` runtime crate (Rust)
│   ├── Cargo.toml
│   └── src/lib.rs
└── docs/
    ├── design.md                     # software design description
    └── architecture.md               # tooling overview
```

## Setup

1. Make `fprime-rust-ac` available on your `PATH`:

   ```bash
   pip install fprime-rust  # once published; today: pip install -e fprime-rust
   ```

2. Reference the library from your deployment's `settings.ini`:

   ```ini
   [fprime]
   library_locations: ./lib/fprime-rust
   ```

3. Register the target in your project's top-level `CMakeLists.txt`:

   ```cmake
   register_fprime_target(target/fprime_rust)
   ```

   (Alternatively, mirror the example in
   [`Ref/CMakeLists.txt`](../Ref/CMakeLists.txt) which adds the rust example
   under the standard Ref deployment.)

4. Author components: add `@ fprime-rust` to the FPP component block.  After
   the first build, edit the seeded `<Component>.rs` to fill in behavior.

5. Build with `fprime-util generate && fprime-util build` as usual.  CMake
   will invoke `fprime-rust-ac` and `cargo build --release`.

## Sample component

A minimal example lives in
[`Ref/RustExample/`](../Ref/RustExample/).  It demonstrates one of each:

- a command (`Reset`),
- an event (`ResetIssued`),
- a telemetry channel (`Counter`),
- a parameter (`Threshold`).

## Documentation

- [`docs/design.md`](docs/design.md) -- detailed software design description.
- [`docs/architecture.md`](docs/architecture.md) -- block-level overview and
  data flow.

## Limitations / future work

- **Type support**: currently primitive FPP types only (`U8`-`U64`, `I8`-`I64`,
  `F32`/`F64`, `bool`).  Strings, arrays, enums, and structs are tracked in
  `docs/design.md`.
- **Async commands**: command-handler dispatch is synchronous from the Rust
  side; long-running operations should off-load to dedicated worker tasks.
- **Unit tests**: the autocoder has Python unit tests
  (`pytest fprime-rust/tests`) but the Rust integration test harness is not
  yet wired into FPP-generated testers.
- **Build verification**: the autocoder is wired into CMake, but a full
  end-to-end build has not been run on this branch (an FPP/fprime-tools
  environment was not available in this session).  See the PR description
  for the artifacts that *were* validated.

## License

Apache 2.0.  Same as the rest of F Prime.
