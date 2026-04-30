# RustExample Component -- Software Design Description

## 1. Purpose

`RustExample` is a reference component that exists solely to exercise the
[`fprime-rust`](../../../fprime-rust/README.md) autocoder.  It is functionally
trivial -- a saturating counter with reset/bump commands -- but covers the
full surface of FPP features the autocoder supports today.

## 2. Interface

### 2.1 Commands

| Opcode | Name    | Args         | Description                                    |
|--------|---------|--------------|------------------------------------------------|
| 0      | Reset   | --           | Reset the counter to zero and emit telemetry. |
| 1      | Bump    | amount: U32  | Add `amount`, saturating at `Threshold`.       |

### 2.2 Events

| Id | Name        | Args        | Severity      | Description                          |
|----|-------------|-------------|---------------|--------------------------------------|
| 0  | ResetIssued | --          | activity low  | Logged on every successful Reset.    |
| 1  | Bumped      | value: U32  | activity low  | Logged on every successful Bump.     |

### 2.3 Telemetry

| Id | Channel | Type | Description                |
|----|---------|------|----------------------------|
| 0  | Counter | U32  | Current value of the counter. |

### 2.4 Parameters

| Id | Parameter | Type | Default | Description                                    |
|----|-----------|------|---------|------------------------------------------------|
| 0  | Threshold | U32  | 100     | Saturation cap applied to the running counter. |

## 3. Behavior

The component holds a single `u32` counter inside its Rust-side state.  On
`Reset` the counter is zeroed; on `Bump(amount)` the counter is increased by
`amount` and clamped to the current `Threshold` parameter value.  Both
commands publish the new counter value on the `Counter` telemetry channel and
log a corresponding event.

## 4. Implementation notes

- The C++ side of this component is purely auto-generated.  Only
  `RustExample.fpp` and `RustExample.rs` live in the source tree.
- The component is `queued` so command handlers run on the component's
  scheduler thread.  The Rust trait dispatch holds `&mut self`, so no
  internal locking is required.
- `param_Threshold` is queried inside `Bump_cmd_handler` rather than at
  startup, so live parameter overlays take effect immediately on the next
  command.

## 5. Testing

- The auto-coded crate `fprime_rust_rust_example` builds standalone with
  `cargo build --release` once the parent deployment generation has run.
- The Python autocoder ships unit tests
  (`pytest fprime-rust/tests`) that include `RustExample.fpp` as a fixture
  and assert on the rendered C++ / Rust files.
