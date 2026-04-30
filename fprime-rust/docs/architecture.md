# fprime-rust Architecture (one-page overview)

This page is the short visual companion to
[`design.md`](design.md).  Use it to quickly orient new contributors; refer
to the SDD for detailed contracts.

## At a glance

- **In:** an FPP component annotated with `@ fprime-rust` plus a hand-written
  `<Component>.rs` implementing the auto-generated trait.
- **Out:** a regular F Prime C++ static library that links against a Rust
  staticlib produced by `cargo build --release`.

## Pipeline

```
        FPP                    cargo                  cmake/ninja
   ┌─────────────┐         ┌───────────┐            ┌───────────┐
   │  Foo.fpp    │         │ rustc     │            │  link     │
   │  @ rust     │         │ stable    │            │           │
   └─────┬───────┘         └────┬──────┘            └─────┬─────┘
         │                      │                         │
         ▼                      ▼                         ▼
 ┌────────────────┐   ┌──────────────────────┐   ┌──────────────────┐
 │ FooBase (C++)  │   │ libfprime_rust_foo.a │   │  Deployment ELF  │
 └────┬───────────┘   └──────────────────────┘   └────────────┬─────┘
      │                                                       │
      ▼                                                       │
 ┌──────────────────┐                                         │
 │ FooRustImpl.cpp  ├─────────────────► extern "C" FFI ───────┘
 └──────────────────┘
```

## File map (per annotated component `Foo`)

| Path                                      | Owner       | Regenerated each build? |
|-------------------------------------------|-------------|--------------------------|
| `Foo.fpp`                                 | User        | No                       |
| `Foo.rs`                                  | User        | No (seeded once)         |
| build/.../`FooRustImpl.{hpp,cpp}`         | Autocoder   | Yes                      |
| build/.../`foo_base.rs`                   | Autocoder   | Yes                      |
| build/.../`Foo.template.rs`               | Autocoder   | Yes                      |
| build/.../`_fprime_rust_crate/foo_crate/` | Autocoder   | Yes                      |
| `target/release/libfprime_rust_foo.a`     | cargo       | On Rust source change    |

## Cross-language ABI surface

For a component `Foo`, the autocoder emits exactly these symbols:

| Symbol                           | Direction | Owner   |
|----------------------------------|-----------|---------|
| `rust_foo_new`                   | C -> Rust | Rust    |
| `rust_foo_free`                  | C -> Rust | Rust    |
| `rust_foo_cmd_<Cmd>`             | C -> Rust | Rust    |
| `rust_foo_tlm_<Channel>`         | Rust -> C | C++     |
| `rust_foo_event_<Event>`         | Rust -> C | C++     |
| `rust_foo_param_<Param>`         | Rust -> C | C++     |
| `rust_foo_cmdResponse`           | Rust -> C | C++     |

All argument types are FPP primitives (`U8`-`U64`, `I8`-`I64`, `F32`, `F64`,
`bool`).  This is the entire surface a security review needs to audit per
component.
