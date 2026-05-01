"""Tests for the fprime-rust-ac CLI."""

from __future__ import annotations

from pathlib import Path

from fprime_rust.autocode.__main__ import main

REF_FPP = (
    Path(__file__).resolve().parents[2] / "Ref" / "RustExample" / "RustExample.fpp"
)


def test_bindings_dry_run_prints_paths(tmp_path: Path, capsys) -> None:
    rc = main(
        [
            "bindings",
            "--dry-run",
            "--output-directory",
            str(tmp_path),
            "--translation-units",
            str(REF_FPP),
        ]
    )
    assert rc == 0
    stdout = capsys.readouterr().out.strip().split()
    names = sorted(Path(p).name for p in stdout)
    assert names == [
        "RustExample.cpp",
        "RustExample.hpp",
        "RustExample.template.rs",
        "rust_example_base.rs",
    ]
    # Dry run must not write anything
    assert list(tmp_path.iterdir()) == []


def test_bindings_writes_expected_files(tmp_path: Path) -> None:
    main(
        [
            "bindings",
            "--output-directory",
            str(tmp_path),
            "--translation-units",
            str(REF_FPP),
        ]
    )
    files = sorted(p.name for p in tmp_path.iterdir())
    assert files == [
        "RustExample.cpp",
        "RustExample.hpp",
        "RustExample.template.rs",
        "rust_example_base.rs",
    ]
    text = (tmp_path / "RustExample.cpp").read_text()
    assert "rust_rust_example_cmd_Reset" in text


def test_crate_generator(tmp_path: Path) -> None:
    runtime = tmp_path / "runtime"
    runtime.mkdir()
    user_dir = tmp_path / "user"
    user_dir.mkdir()
    output_dir = tmp_path / "out"
    rc = main(
        [
            "crate",
            "--translation-units",
            str(REF_FPP),
            "--output-directory",
            str(output_dir),
            "--user-dir",
            str(user_dir),
            "--runtime-path",
            str(runtime),
        ]
    )
    assert rc == 0
    crate_dir = output_dir / "rust_example_crate"
    assert (crate_dir / "Cargo.toml").exists()
    assert (crate_dir / "lib.rs").exists()
    assert (crate_dir / "rust-toolchain.toml").exists()
