"""Tests for the lightweight FPP parser used by fprime-rust."""

from __future__ import annotations

from pathlib import Path

import pytest

from fprime_rust.autocode.fpp_parser import (
    FppCommand,
    FppEvent,
    FppParameter,
    FppTelemetry,
    parse_fpp_file,
)

REF_FPP = (
    Path(__file__).resolve().parents[2] / "Ref" / "RustExample" / "RustExample.fpp"
)


def test_ref_fpp_yields_single_annotated_component() -> None:
    components = parse_fpp_file(REF_FPP)
    assert len(components) == 1
    component = components[0]
    assert component.name == "RustExample"
    assert component.kind == "queued"
    assert component.namespace == ["Ref"]
    assert component.cpp_namespace == "Ref"
    assert component.cpp_fqn == "Ref::RustExample"
    assert component.rust_module == "rust_example"


def test_commands_are_extracted_with_args() -> None:
    component = parse_fpp_file(REF_FPP)[0]
    names = sorted(c.name for c in component.commands)
    assert names == ["Bump", "Reset"]
    bump = next(c for c in component.commands if c.name == "Bump")
    assert bump.kind == "async"
    assert [(a.name, a.fpp_type) for a in bump.args] == [("amount", "U32")]


def test_events_are_extracted_with_args() -> None:
    component = parse_fpp_file(REF_FPP)[0]
    by_name = {e.name: e for e in component.events}
    assert set(by_name) == {"ResetIssued", "Bumped"}
    assert by_name["Bumped"].args[0].name == "value"
    assert by_name["Bumped"].args[0].fpp_type == "U32"
    assert by_name["ResetIssued"].args == []


def test_telemetry_and_params_extracted() -> None:
    component = parse_fpp_file(REF_FPP)[0]
    assert [(t.name, t.fpp_type) for t in component.telemetry] == [("Counter", "U32")]
    assert [(p.name, p.fpp_type) for p in component.parameters] == [
        ("Threshold", "U32")
    ]


def test_unannotated_component_is_skipped(tmp_path: Path) -> None:
    fpp = tmp_path / "Plain.fpp"
    fpp.write_text("""
        module Bar {
            @ Just a regular component, no rust annotation.
            passive component Plain {
                async command DoIt
            }
        }
        """)
    assert parse_fpp_file(fpp) == []


def test_nested_module_namespace_is_captured(tmp_path: Path) -> None:
    fpp = tmp_path / "Nested.fpp"
    fpp.write_text("""
        module Outer {
            module Inner {
                @ fprime-rust
                passive component Hello {
                    sync command Ping
                }
            }
        }
        """)
    components = parse_fpp_file(fpp)
    assert len(components) == 1
    assert components[0].namespace == ["Outer", "Inner"]
    assert components[0].cpp_fqn == "Outer::Inner::Hello"


def test_arguments_with_continuations(tmp_path: Path) -> None:
    fpp = tmp_path / "Cont.fpp"
    fpp.write_text("""
        module M {
            @ fprime-rust
            active component K {
                async command Multi(
                                       a: U8
                                       b: I32
                                       c: F64
                                     ) \
                  opcode 0
            }
        }
        """)
    component = parse_fpp_file(fpp)[0]
    multi = component.commands[0]
    assert [(a.name, a.fpp_type) for a in multi.args] == [
        ("a", "U8"),
        ("b", "I32"),
        ("c", "F64"),
    ]


@pytest.mark.parametrize(
    "annot_block,expected",
    [
        ("@ fprime-rust\n", True),
        ("@ Some other note\n@ fprime-rust\n", True),
        ("@ Plain note\n", False),
        ("", False),
    ],
)
def test_annotation_detection(tmp_path: Path, annot_block: str, expected: bool) -> None:
    fpp = tmp_path / "T.fpp"
    fpp.write_text(f"""
        module M {{
            {annot_block}            passive component C {{ }}
        }}
        """)
    assert (len(parse_fpp_file(fpp)) == 1) is expected
