//! `fprime-rust` -- runtime support crate for Rust-backed F Prime components.
//!
//! This crate is intentionally small: it only contains the types that are
//! shared across every auto-generated component crate.  The actual FFI symbols
//! are declared in the per-component generated `<comp>_base.rs` modules so
//! that they can match the unique-by-component C symbols in the C++ shim.
//!
//! ## Threading model
//! Component handlers are dispatched by F Prime on whatever scheduling thread
//! the topology assigns to the component's queue.  All public types in this
//! crate are therefore `Send`; they are *not* `Sync` and must not be shared
//! across handler invocations except through `&mut self` on the user struct.
//!
//! ## Stability
//! The Rust API surface here is exposed to user-authored components and is
//! covered by the same compatibility expectations as the rest of fprime-rust.
//! The FFI layer between this crate and the C++ shim is an implementation
//! detail and may change without notice.
#![deny(missing_docs)]
#![deny(unsafe_op_in_unsafe_fn)]

use core::ffi::c_void;

/// Final response status returned to the F Prime command dispatcher.
///
/// Mirrors `Fw::CmdResponse::T` in the F Prime C++ headers.
#[repr(u8)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum CmdResponse {
    /// Command succeeded.
    Ok = 0,
    /// Command was malformed (e.g. arguments did not deserialize).
    InvalidOpcode = 1,
    /// Command parameters were out of range.
    ValidationError = 2,
    /// Command receiver was disabled or in the wrong state.
    FormatError = 3,
    /// Command runtime had an unexpected failure.
    ExecutionError = 4,
    /// Command was rejected by upstream filtering / authorization.
    BusyError = 5,
}

/// Validity flag returned alongside parameter reads.
///
/// Mirrors `Fw::ParamValid::T`.  Use [`ParamValid::from_raw`] to decode the
/// `u8` returned by the FFI layer.
#[repr(u8)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum ParamValid {
    /// Parameter has not been initialized.
    Uninit = 0,
    /// Parameter is valid and was read successfully.
    Valid = 1,
    /// Parameter has been initialized but its store reports an error.
    Invalid = 2,
    /// Parameter was loaded from the default in code (no overlay value).
    Default = 3,
    /// Parameter validity is unknown.
    Unknown = 4,
}

impl ParamValid {
    /// Decode a raw byte returned by the C++ side into a `ParamValid` enum.
    ///
    /// Unknown values are mapped to [`ParamValid::Unknown`] rather than
    /// panicking so a runtime mismatch with the C++ side fails closed.
    pub fn from_raw(raw: u8) -> Self {
        match raw {
            0 => ParamValid::Uninit,
            1 => ParamValid::Valid,
            2 => ParamValid::Invalid,
            3 => ParamValid::Default,
            _ => ParamValid::Unknown,
        }
    }
}

/// Per-handler context handed to component code.
///
/// The context owns a non-null pointer to the C++ component instance and the
/// arguments needed to format a command response (`op_code`, `cmd_seq`).  All
/// FFI calls go through this type so the unsafe surface is contained here.
pub struct Context {
    /// Opaque pointer to the C++ component instance (`<Comp> *`).
    pub cpp_self: *mut c_void,
    /// Opcode of the in-flight command.  Set to 0 for non-command handlers.
    pub op_code: u32,
    /// Sequence number of the in-flight command.  Set to 0 for non-command
    /// handlers.
    pub cmd_seq: u32,
}

// SAFETY: Context only carries primitives plus an opaque pointer that the
// runtime treats as inert until handed back to the C++ side via FFI.  The
// pointer outlives the handler invocation because it is owned by the running
// component instance.  We do not mark Sync; concurrent handler invocations are
// not supported.
unsafe impl Send for Context {}

impl Context {
    /// Construct a new context with the given C++ instance and command IDs.
    pub fn new(cpp_self: *mut c_void, op_code: u32, cmd_seq: u32) -> Self {
        Self { cpp_self, op_code, cmd_seq }
    }

    /// Return the raw C++ instance pointer.
    pub fn raw_self(&self) -> *mut c_void {
        self.cpp_self
    }
}
