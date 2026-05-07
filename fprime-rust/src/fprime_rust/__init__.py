"""fprime_rust: F Prime to Rust autocoder.

The :mod:`fprime_rust` Python package provides the build-time autocoder
``fprime-rust-ac`` and supporting CMake glue.  Component authors annotate an
FPP component with ``@ fprime-rust`` and the autocoder produces:

* a C++ "shim" implementation that derives from the FPP-generated
  ``<Component>ComponentBase`` and forwards every command/port handler to a
  Rust function via an ``extern "C"`` FFI;
* a Rust "base" module declaring the corresponding handler trait, FFI
  declarations for the F Prime side (telemetry, events, command response,
  parameter access, ...), and a ``unsafe extern "C"`` glue layer; and
* a one-time-generated ``<Component>.template.rs`` user template.

See the top-level ``README.md`` and ``docs/design.md`` for the architecture
description.
"""

__version__ = "0.1.0"
