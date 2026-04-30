"""Entry point for the ``fprime-rust-ac`` CLI.

This tool is invoked exclusively from CMake (see
``cmake/autocoder/fprime_rust.cmake``).  It supports two subcommands:

``bindings``
    Process one or more FPP translation units and emit the C++ shim plus
    Rust base/user-template files for every ``@ fprime-rust`` annotated
    component.  Supports a ``--dry-run`` mode that just prints the file
    paths it *would* generate (used by CMake to set up custom commands).

``crate``
    Materialize the per-component Cargo workspace (``Cargo.toml``,
    ``lib.rs``, ``rust-toolchain.toml``) for an annotated component.

The CLI is intentionally minimal: nothing it does requires network access or
mutable state outside the supplied output directory.
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import asdict
from pathlib import Path
from typing import Iterable, List, Sequence

from .cargo_generator import render_cargo_toml, render_lib_rs, render_toolchain
from .cpp_generator import generate_cpp
from .fpp_parser import FppComponent, parse_fpp_file
from .rust_generator import generate_rust

# --------------------------------------------------------------------------
# Helpers
# --------------------------------------------------------------------------


def _write_file(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def _list_components(translation_units: Iterable[Path]) -> List[FppComponent]:
    components: List[FppComponent] = []
    for tu in translation_units:
        components.extend(parse_fpp_file(tu))
    return components


# --------------------------------------------------------------------------
# Subcommands
# --------------------------------------------------------------------------


def _cmd_bindings(args: argparse.Namespace) -> int:
    components = _list_components(args.translation_units)
    if not components and not args.dry_run:
        # Not an error: CMake passes every module's FPP through the autocoder
        # and most modules will not be Rust-backed.
        print("", end="")
        return 0

    output: List[Path] = []
    for comp in components:
        cpp = generate_cpp(comp, args.output_directory)
        rust = generate_rust(comp, args.output_directory)
        output.extend(
            [cpp.header_path, cpp.source_path, rust.base_path, rust.template_path]
        )
        if not args.dry_run:
            _write_file(cpp.header_path, cpp.header)
            _write_file(cpp.source_path, cpp.source)
            _write_file(rust.base_path, rust.base)
            _write_file(rust.template_path, rust.template)
    print(" ".join(str(p) for p in output))
    return 0


def _cmd_crate(args: argparse.Namespace) -> int:
    components = _list_components(args.translation_units)
    if not components:
        return 0
    runtime_path = args.runtime_path.resolve()
    bindings_dir = (
        args.bindings_dir.resolve()
        if args.bindings_dir
        else args.output_directory.resolve()
    )
    output: List[Path] = []
    for comp in components:
        crate_dir = args.output_directory / f"{comp.rust_module}_crate"
        cargo_toml = crate_dir / "Cargo.toml"
        lib_rs = crate_dir / "lib.rs"
        toolchain = crate_dir / "rust-toolchain.toml"
        user_rs = (args.user_dir / f"{comp.name}.rs").resolve()
        base_rs = bindings_dir / f"{comp.rust_module}_base.rs"
        if not args.dry_run:
            _write_file(cargo_toml, render_cargo_toml(comp, runtime_path))
            _write_file(lib_rs, render_lib_rs(comp, user_rs, base_rs))
            _write_file(toolchain, render_toolchain())
        output.extend([cargo_toml, lib_rs, toolchain])
    print(" ".join(str(p) for p in output))
    return 0


# --------------------------------------------------------------------------
# Argument parser
# --------------------------------------------------------------------------


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="fprime-rust-ac",
        description="F Prime to Rust autocoder.",
    )
    sub = parser.add_subparsers(dest="command", required=True)

    bindings = sub.add_parser(
        "bindings",
        help="Generate C++ shim and Rust base/template files for annotated components.",
    )
    bindings.add_argument(
        "--translation-units",
        nargs="+",
        required=True,
        type=Path,
        metavar="FPP",
        help="FPP translation units to scan for @ fprime-rust components.",
    )
    bindings.add_argument(
        "--output-directory",
        required=True,
        type=Path,
        help="Directory the autocoder writes generated files into.",
    )
    bindings.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the file paths that would be generated without writing them.",
    )
    bindings.set_defaults(func=_cmd_bindings)

    crate = sub.add_parser(
        "crate",
        help="Generate the per-component Cargo workspace.",
    )
    crate.add_argument(
        "--translation-units",
        nargs="+",
        required=True,
        type=Path,
        metavar="FPP",
    )
    crate.add_argument(
        "--output-directory",
        required=True,
        type=Path,
    )
    crate.add_argument(
        "--user-dir",
        required=True,
        type=Path,
        help="Directory containing the user's <Component>.rs files.",
    )
    crate.add_argument(
        "--runtime-path",
        required=True,
        type=Path,
        help="Path to the fprime-rust runtime crate (relative paths are resolved).",
    )
    crate.add_argument(
        "--bindings-dir",
        type=Path,
        default=None,
        help=(
            "Directory containing the auto-generated <comp>_base.rs files.  "
            "Defaults to --output-directory."
        ),
    )
    crate.add_argument("--dry-run", action="store_true")
    crate.set_defaults(func=_cmd_crate)

    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(list(argv) if argv is not None else None)
    return args.func(args)


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
