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
    assert component.kind == "active"
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
    # Severity comes through the line-continued ``severity activity low``
    # clause and is normalized to snake_case.
    assert by_name["ResetIssued"].severity == "activity_low"
    assert by_name["Bumped"].severity == "activity_low"


@pytest.mark.parametrize(
    "fpp_severity,expected",
    [
        ("severity activity low", "activity_low"),
        ("severity activity high", "activity_high"),
        ("severity warning low", "warning_low"),
        ("severity warning high", "warning_high"),
        ("severity command", "command"),
        ("severity fatal", "fatal"),
        ("severity diagnostic", "diagnostic"),
        ("", "activity_low"),  # default when omitted
    ],
)
def test_event_severity_parsing(tmp_path: Path, fpp_severity: str, expected: str) -> None:
    fpp = tmp_path / "S.fpp"
    fpp.write_text(f"""
        module M {{
            @ fprime-rust
            passive component C {{
                event Boom {fpp_severity} id 0 format \"x\"
            }}
        }}
        """)
    component = parse_fpp_file(fpp)[0]
    assert component.events[0].severity == expected


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


@pytest.mark.parametrize("kind", ["sync", "async", "guarded"])
def test_typed_input_port_raises_clear_error(tmp_path: Path, kind: str) -> None:
    """Typed input ports on a Rust-backed component would generate a
    pure-virtual handler in the base class that the MVP autocoder cannot
    override.  The parser must reject them up front with a clear message
    rather than letting the user hit a confusing C++ error.
    """
    fpp = tmp_path / f"{kind}.fpp"
    fpp.write_text(f"""
        module M {{
            @ fprime-rust
            queued component Q {{
                {kind} input port run: Svc.Sched
                async command Reset opcode 0
            }}
        }}
        """)
    from fprime_rust.autocode.fpp_parser import UnsupportedFppFeatureError
    with pytest.raises(UnsupportedFppFeatureError) as excinfo:
        parse_fpp_file(fpp)
    msg = str(excinfo.value)
    assert "Q" in msg
    assert "run" in msg
    assert "MVP" in msg


def test_typed_input_port_on_unannotated_component_is_ignored(tmp_path: Path) -> None:
    """Components without ``@ fprime-rust`` are not our problem -- they go
    through the regular C++ path -- so an input port there must NOT trigger
    the MVP guard.
    """
    fpp = tmp_path / "plain.fpp"
    fpp.write_text("""
        module M {
            passive component PlainCpp {
                sync input port run: Svc.Sched
            }
            @ fprime-rust
            queued component RustOne {
                async command Ping opcode 0
            }
        }
        """)
    components = parse_fpp_file(fpp)
    assert [c.name for c in components] == ["RustOne"]


def test_format_string_with_hash_does_not_corrupt_parse(tmp_path: Path) -> None:
    """Regression: ``#`` and ``//`` inside quoted strings must not be treated
    as comment starts -- otherwise the closing quote is eaten and brace
    matching skips into a different declaration.
    """
    fpp = tmp_path / "Quoted.fpp"
    fpp.write_text("""
        module Z {
            @ fprime-rust
            passive component Q {
                event Hashed severity warning low id 0 format "code: #42"
                event UrlIsh severity warning high id 1 format "see https://x"
                telemetry T: U32 id 0
                param P: U32 default 1 id 0
            }
        }
        """)
    components = parse_fpp_file(fpp)
    assert len(components) == 1
    c = components[0]
    by_name = {e.name: e for e in c.events}
    assert set(by_name) == {"Hashed", "UrlIsh"}
    assert by_name["Hashed"].severity == "warning_low"
    assert by_name["UrlIsh"].severity == "warning_high"
    assert [t.name for t in c.telemetry] == ["T"]
    assert [p.name for p in c.parameters] == ["P"]


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
