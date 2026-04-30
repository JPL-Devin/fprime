"""Tests for the fprime-rust C++ / Rust generators."""

from __future__ import annotations

from pathlib import Path

import pytest

from fprime_rust.autocode.cargo_generator import (
    render_cargo_toml,
    render_lib_rs,
    render_toolchain,
)
from fprime_rust.autocode.cpp_generator import render_header, render_source
from fprime_rust.autocode.fpp_parser import parse_fpp_file
from fprime_rust.autocode.rust_generator import render_base_module, render_user_template

REF_FPP = (
    Path(__file__).resolve().parents[2] / "Ref" / "RustExample" / "RustExample.fpp"
)


@pytest.fixture(scope="module")
def component():
    return parse_fpp_file(REF_FPP)[0]


def test_cpp_header_extends_base_class(component) -> None:
    header = render_header(component)
    assert "class RustExample final : public RustExampleComponentBase" in header
    assert "void Reset_cmdHandler" in header
    assert "void Bump_cmdHandler" in header
    assert "void* m_rust_impl" in header
    assert "FPRIME_RUST_RUST_EXAMPLE_HPP" in header
    # Namespaces are preserved in declaration order
    assert "namespace Ref {" in header
    assert "}  // namespace Ref" in header


def test_cpp_source_forwards_to_rust(component) -> None:
    source = render_source(component)
    # Inbound dispatch: cmdHandler bodies call rust_<comp>_cmd_<name>
    assert "::rust_rust_example_cmd_Reset(this->m_rust_impl, opCode, cmdSeq)" in source
    assert (
        "::rust_rust_example_cmd_Bump(this->m_rust_impl, opCode, cmdSeq, amount)"
        in source
    )
    # Outbound sinks for telemetry / event / param / cmdResponse
    assert "rust_rust_example_tlm_Counter" in source
    assert "rust_rust_example_event_ResetIssued" in source
    assert "rust_rust_example_event_Bumped" in source
    assert "rust_rust_example_param_Threshold" in source
    assert "rust_rust_example_cmdResponse" in source
    # Constructor allocates the Rust-side state
    assert "rust_rust_example_new()" in source
    assert "rust_rust_example_free(this->m_rust_impl)" in source


def test_rust_base_module(component) -> None:
    base = render_base_module(component)
    assert "pub trait Component" in base
    assert "fn Reset_cmd_handler(&mut self, ctx: &mut Context);" in base
    assert "fn Bump_cmd_handler(&mut self, ctx: &mut Context, amount: u32);" in base
    # FFI declarations for sinks
    assert "pub fn rust_rust_example_tlm_Counter" in base
    assert "pub fn rust_rust_example_event_Bumped" in base
    assert "pub fn rust_rust_example_param_Threshold" in base
    # Inbound dispatch shims
    assert 'pub unsafe extern "C" fn rust_rust_example_new' in base
    assert 'pub unsafe extern "C" fn rust_rust_example_cmd_Bump' in base
    # Helper methods on Context
    assert "pub fn tlm_write_Counter" in base
    assert "pub fn log_Bumped(&self, value: u32)" in base
    assert "pub fn param_Threshold" in base


def test_rust_user_template(component) -> None:
    template = render_user_template(component)
    assert "pub struct Implementation" in template
    assert "impl Component for Implementation" in template
    # Includes a stub body for both commands
    assert "fn Reset_cmd_handler" in template
    assert "fn Bump_cmd_handler" in template
    assert "ctx.command_response(CmdResponse::Ok)" in template


def test_cargo_artifacts(component, tmp_path: Path) -> None:
    runtime = tmp_path / "runtime"
    runtime.mkdir()
    user_rs = tmp_path / "RustExample.rs"
    user_rs.write_text("// stub")

    cargo_toml = render_cargo_toml(component, runtime)
    assert 'name = "fprime_rust_rust_example"' in cargo_toml
    assert 'crate-type = ["staticlib"]' in cargo_toml
    assert "fprime-rust =" in cargo_toml
    assert runtime.as_posix() in cargo_toml

    base_rs = tmp_path / "rust_example_base.rs"
    base_rs.write_text("// stub")
    lib_rs = render_lib_rs(component, user_rs, base_rs)
    assert "pub mod rust_example_base;" in lib_rs
    assert "pub mod rust_example;" in lib_rs
    assert base_rs.as_posix() in lib_rs
    assert user_rs.as_posix() in lib_rs

    assert "stable" in render_toolchain()
