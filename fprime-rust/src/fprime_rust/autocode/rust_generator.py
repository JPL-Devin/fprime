"""Generate the Rust-side artefacts for a Rust-backed F Prime component.

Two files are produced per annotated component:

* ``<component>_base.rs`` -- the *generated* trait declaration, FFI extern
  blocks for the C++ "sink" functions, and the ``#[no_mangle] extern "C"``
  shims that dispatch incoming command handlers to the user's trait
  implementation.  This file is regenerated on every build and **must not be
  edited by hand**.

* ``<Component>.template.rs`` -- a one-shot copy that lands in the source
  tree as ``<Component>.rs`` for the user to fill in.  The Python autocoder
  CMake target only copies the template if ``<Component>.rs`` does not
  already exist, mirroring the convention from ``fprime-python``.

The generated module name is the ``snake_case`` form of the component name to
keep with Rust naming conventions (e.g. ``RustExample`` -> ``rust_example``).
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import List

from .fpp_parser import FppArg, FppComponent


def _ffi_name(component: FppComponent, suffix: str) -> str:
    return f"rust_{component.rust_module}_{suffix}"


def _rust_arg_list(args: List[FppArg]) -> str:
    return ", ".join(f"{a.name}: {a.rust_type}" for a in args)


def _rust_arg_names(args: List[FppArg]) -> str:
    return ", ".join(a.name for a in args)


def render_base_module(component: FppComponent) -> str:
    """Render ``<component>_base.rs`` (the build-cache-side base module)."""
    name = component.name
    snake = component.rust_module
    cpp_fqn = component.cpp_fqn

    # Trait method declarations for command handlers
    handler_decls: List[str] = []
    for cmd in component.commands:
        sig = f"    fn {cmd.name}_cmd_handler(&mut self, ctx: &mut Context, {_rust_arg_list(cmd.args)});"
        if not cmd.args:
            sig = f"    fn {cmd.name}_cmd_handler(&mut self, ctx: &mut Context);"
        handler_decls.append(sig)

    # FFI extern blocks for sinks defined on the C++ side
    extern_decls: List[str] = []
    for ch in component.telemetry:
        sym = _ffi_name(component, f"tlm_{ch.name}")
        extern_decls.append(
            f"    pub fn {sym}(fp_self: *mut c_void, value: {ch.rust_type});"
        )
    for ev in component.events:
        sym = _ffi_name(component, f"event_{ev.name}")
        if ev.args:
            extern_decls.append(
                f"    pub fn {sym}(fp_self: *mut c_void, {_rust_arg_list(ev.args)});"
            )
        else:
            extern_decls.append(f"    pub fn {sym}(fp_self: *mut c_void);")
    for prm in component.parameters:
        sym = _ffi_name(component, f"param_{prm.name}")
        extern_decls.append(
            f"    pub fn {sym}(fp_self: *mut c_void, out_valid: *mut u8) -> {prm.rust_type};"
        )
    cmd_resp_sym = _ffi_name(component, "cmdResponse")
    extern_decls.append(
        f"    pub fn {cmd_resp_sym}(fp_self: *mut c_void, op_code: u32, cmd_seq: u32, status: u8);"
    )

    # Inbound dispatch: extern "C" Rust functions that the C++ shim calls
    dispatch_fns: List[str] = []
    new_sym = _ffi_name(component, "new")
    free_sym = _ffi_name(component, "free")
    dispatch_fns.append(
        f"""/// Constructor: allocate the user-supplied component on the heap and return
/// an opaque pointer the C++ side will hold for the lifetime of the
/// component.
#[no_mangle]
pub unsafe extern \"C\" fn {new_sym}() -> *mut c_void {{
    let boxed: Box<dyn Component> = Box::new(crate::{snake}::Implementation::new());
    let raw: *mut Box<dyn Component> = Box::into_raw(Box::new(boxed));
    raw as *mut c_void
}}

/// Destructor counterpart to ``{new_sym}``.  Called from the C++
/// destructor; safe to invoke with a null pointer for symmetry.
#[no_mangle]
pub unsafe extern \"C\" fn {free_sym}(self_ptr: *mut c_void) {{
    if self_ptr.is_null() {{
        return;
    }}
    let _ = Box::from_raw(self_ptr as *mut Box<dyn Component>);
}}"""
    )

    for cmd in component.commands:
        sym = _ffi_name(component, f"cmd_{cmd.name}")
        c_args = [
            FppArg("self_ptr", "*mut c_void"),
            FppArg("op_code", "u32"),
            FppArg("cmd_seq", "u32"),
        ] + [FppArg(a.name, a.rust_type) for a in cmd.args]
        forwarded = _rust_arg_names(cmd.args)
        forwarded_call = f", {forwarded}" if forwarded else ""
        dispatch_fns.append(f"""#[no_mangle]
pub unsafe extern \"C\" fn {sym}({_rust_arg_list(c_args)}) {{
    if self_ptr.is_null() {{
        return;
    }}
    let boxed: &mut Box<dyn Component> = &mut *(self_ptr as *mut Box<dyn Component>);
    let mut ctx = Context::new(self_ptr, op_code, cmd_seq);
    boxed.{cmd.name}_cmd_handler(&mut ctx{forwarded_call});
}}""")

    # Telemetry/event/parameter helpers exposed on Context
    ctx_methods: List[str] = []
    for ch in component.telemetry:
        sym = _ffi_name(component, f"tlm_{ch.name}")
        ctx_methods.append(
            f"""    /// Publish a value on the ``{ch.name}`` telemetry channel.
    pub fn tlm_write_{ch.name}(&self, value: {ch.rust_type}) {{
        unsafe {{ ffi::{sym}(self.cpp_self, value) }};
    }}"""
        )
    for ev in component.events:
        sym = _ffi_name(component, f"event_{ev.name}")
        ctx_methods.append(f"""    /// Log the ``{ev.name}`` event.
    pub fn log_{ev.name}(&self{', ' + _rust_arg_list(ev.args) if ev.args else ''}) {{
        unsafe {{ ffi::{sym}(self.cpp_self{', ' + _rust_arg_names(ev.args) if ev.args else ''}) }};
    }}""")
    for prm in component.parameters:
        sym = _ffi_name(component, f"param_{prm.name}")
        ctx_methods.append(
            f"""    /// Fetch the latest committed value of ``{prm.name}``.
    pub fn param_{prm.name}(&self) -> ({prm.rust_type}, ParamValid) {{
        let mut valid: u8 = 0;
        let v = unsafe {{ ffi::{sym}(self.cpp_self, &mut valid as *mut u8) }};
        (v, ParamValid::from_raw(valid))
    }}"""
        )

    cmd_resp_method = f"""    /// Return a final command-response code (``OK``, ``EXECUTION_ERROR`` ...).
    pub fn command_response(&self, status: CmdResponse) {{
        unsafe {{ ffi::{cmd_resp_sym}(self.cpp_self, self.op_code, self.cmd_seq, status as u8) }};
    }}"""
    ctx_methods.append(cmd_resp_method)

    rendered_handlers = (
        "\n".join(handler_decls) if handler_decls else "    // (no commands)"
    )
    rendered_externs = "\n".join(extern_decls)
    rendered_dispatch = "\n\n".join(dispatch_fns)
    rendered_ctx = "\n\n".join(ctx_methods)

    return f"""//! Auto-generated Rust base for {cpp_fqn}.
//!
//! This file is regenerated by ``fprime-rust-ac`` on every build and **must
//! not be edited by hand**.  The user's behavior lives in the sibling
//! ``{name}.rs`` file, which implements the [`Component`] trait below.

#![allow(non_snake_case)]
#![allow(unused_imports)]

use core::ffi::c_void;
use fprime_rust::{{ParamValid, CmdResponse}};

/// Per-handler context handed to the user's ``Component`` implementation.
///
/// The struct is intentionally **not** a re-export of the runtime ``Context``
/// type so that we can attach component-specific helper methods (one per
/// declared command/event/telemetry/parameter) without running afoul of
/// Rust's orphan rules.
pub struct Context {{
    /// Opaque pointer to the C++ component instance.
    cpp_self: *mut c_void,
    /// Opcode of the in-flight command (zero outside command dispatch).
    op_code: u32,
    /// Sequence number of the in-flight command.
    cmd_seq: u32,
}}

// SAFETY: ``Context`` holds primitives plus an opaque pointer that the C++
// side guarantees outlives the handler call.  We are explicit that it is
// ``Send`` (the F Prime executor moves work between threads) but not
// ``Sync`` (handlers are serialised per component instance).
unsafe impl Send for Context {{}}

impl Context {{
    /// Construct a new context.  Invoked by the auto-generated FFI shims.
    pub fn new(cpp_self: *mut c_void, op_code: u32, cmd_seq: u32) -> Self {{
        Self {{ cpp_self, op_code, cmd_seq }}
    }}

    /// Return the raw C++ instance pointer (for advanced extensions).
    pub fn raw_self(&self) -> *mut c_void {{
        self.cpp_self
    }}

{rendered_ctx}
}}

/// FFI block: every symbol below is implemented by the C++ shim emitted
/// alongside this file.  The signatures are kept layout-compatible with the
/// matching C++ declarations.
mod ffi {{
    use core::ffi::c_void;
    extern "C" {{
{rendered_externs}
    }}
}}

/// Trait implemented by the user's behavior struct.
pub trait Component: Send {{
{rendered_handlers}
}}

// -------------------------------------------------------------------------
// Inbound FFI: these symbols are called from the C++ shim.
// -------------------------------------------------------------------------
{rendered_dispatch}
"""


def render_user_template(component: FppComponent) -> str:
    """Render the ``<Component>.template.rs`` user-facing template."""
    name = component.name
    snake = component.rust_module
    handler_stubs: List[str] = []
    for cmd in component.commands:
        if cmd.args:
            sig = f"    fn {cmd.name}_cmd_handler(&mut self, ctx: &mut Context, {_rust_arg_list(cmd.args)})"
        else:
            sig = f"    fn {cmd.name}_cmd_handler(&mut self, ctx: &mut Context)"
        handler_stubs.append(f"""{sig} {{
        // TODO: implement {cmd.name} command behavior.
        ctx.command_response(CmdResponse::Ok);
    }}""")
    rendered = "\n\n".join(handler_stubs) if handler_stubs else "    // (no commands)"

    return f"""//! User implementation of the {name} F Prime component.
//!
//! Edit this file to provide the component's behavior.  The companion file
//! ``{snake}_base.rs`` is auto-generated and provides the FFI plumbing,
//! command-handler dispatch, and helpers for telemetry/events/parameters.

use fprime_rust::CmdResponse;
use crate::{snake}_base::{{Component, Context}};

/// User-defined behavior for {name}.  All persistent state lives here.
pub struct Implementation {{
    // TODO: add component state fields.
}}

impl Implementation {{
    /// Construct the user state.  Called by the auto-generated FFI
    /// constructor before the FPP topology is wired.
    pub fn new() -> Self {{
        Self {{
        }}
    }}
}}

impl Component for Implementation {{
{rendered}
}}
"""


@dataclass
class RustArtifacts:
    base_path: Path
    template_path: Path
    base: str
    template: str


def generate_rust(component: FppComponent, output_dir: Path) -> RustArtifacts:
    base = render_base_module(component)
    template = render_user_template(component)
    base_path = output_dir / f"{component.rust_module}_base.rs"
    template_path = output_dir / f"{component.name}.template.rs"
    return RustArtifacts(
        base_path=base_path,
        template_path=template_path,
        base=base,
        template=template,
    )
