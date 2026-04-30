"""Lightweight FPP parser used by the fprime-rust autocoder.

The parser is intentionally narrow in scope: it extracts only the information
the Rust autocoder needs to generate FFI bindings for a component annotated
with ``@ fprime-rust``.  It is *not* a full FPP grammar -- it relies on the
upstream FPP toolchain (``fpp-to-cpp``) to do the real type checking and
component-base generation.

The parser handles:

* ``module Foo { ... }`` blocks with arbitrary nesting (the tail is recovered
  to compute the C++ namespace).
* ``[active|passive|queued] component Name { ... }`` blocks.
* ``@`` annotations attached to the component itself, including the
  ``@ fprime-rust`` opt-in marker.
* Inside the component body:

  * ``[sync|async|guarded] command Name(args) ...``
  * ``event Name(args) ...``
  * ``telemetry Name: type ...``
  * ``param Name: type ...``

Only annotated components are returned; everything else is skipped.

This MVP-style parser avoids a hard dependency on ``fpp-to-json`` or
``fprime-python-model``.  It is robust enough for primitive-typed
commands/events/telemetry/parameters, which is the bound the Rust runtime
currently supports.  For richer types we recommend lowering to ``primitives``
in the FPP model and exposing them through additional handlers.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional, Tuple

import re

FPRIME_RUST_ANNOTATION = "fprime-rust"

# FPP primitive type -> Rust type mapping.  These are the types the Rust
# runtime's FFI layer guarantees layout-compatibility for.  Anything outside
# this set requires hand-written extensions and is rejected by the autocoder.
FPP_TO_RUST_PRIMITIVE = {
    "U8": "u8",
    "U16": "u16",
    "U32": "u32",
    "U64": "u64",
    "I8": "i8",
    "I16": "i16",
    "I32": "i32",
    "I64": "i64",
    "F32": "f32",
    "F64": "f64",
    "bool": "bool",
}

# FPP primitive type -> C++ type.  These mirror the FPP-generated headers.
FPP_TO_CPP_PRIMITIVE = {
    "U8": "U8",
    "U16": "U16",
    "U32": "U32",
    "U64": "U64",
    "I8": "I8",
    "I16": "I16",
    "I32": "I32",
    "I64": "I64",
    "F32": "F32",
    "F64": "F64",
    "bool": "bool",
}


@dataclass
class FppArg:
    """A single formal argument inside a command/event/port."""

    name: str
    fpp_type: str

    @property
    def rust_type(self) -> str:
        return FPP_TO_RUST_PRIMITIVE.get(self.fpp_type, self.fpp_type)

    @property
    def cpp_type(self) -> str:
        return FPP_TO_CPP_PRIMITIVE.get(self.fpp_type, self.fpp_type)


@dataclass
class FppCommand:
    name: str
    kind: str  # "sync" | "async" | "guarded"
    args: List[FppArg] = field(default_factory=list)


@dataclass
class FppEvent:
    name: str
    args: List[FppArg] = field(default_factory=list)
    severity: str = "activity_low"  # default; not used for codegen but useful


@dataclass
class FppTelemetry:
    name: str
    fpp_type: str

    @property
    def rust_type(self) -> str:
        return FPP_TO_RUST_PRIMITIVE.get(self.fpp_type, self.fpp_type)

    @property
    def cpp_type(self) -> str:
        return FPP_TO_CPP_PRIMITIVE.get(self.fpp_type, self.fpp_type)


@dataclass
class FppParameter:
    name: str
    fpp_type: str

    @property
    def rust_type(self) -> str:
        return FPP_TO_RUST_PRIMITIVE.get(self.fpp_type, self.fpp_type)

    @property
    def cpp_type(self) -> str:
        return FPP_TO_CPP_PRIMITIVE.get(self.fpp_type, self.fpp_type)


@dataclass
class FppComponent:
    """A single component annotated with ``@ fprime-rust``."""

    name: str
    kind: str  # "active" | "passive" | "queued"
    namespace: List[str]
    commands: List[FppCommand] = field(default_factory=list)
    events: List[FppEvent] = field(default_factory=list)
    telemetry: List[FppTelemetry] = field(default_factory=list)
    parameters: List[FppParameter] = field(default_factory=list)
    source_file: Optional[Path] = None

    @property
    def cpp_namespace(self) -> str:
        return "::".join(self.namespace) if self.namespace else ""

    @property
    def cpp_fqn(self) -> str:
        return f"{self.cpp_namespace}::{self.name}" if self.cpp_namespace else self.name

    @property
    def rust_module(self) -> str:
        """The Rust crate-side module name (snake-case of the component)."""
        return _camel_to_snake(self.name)


def _camel_to_snake(name: str) -> str:
    """Convert ``CamelCase`` to ``camel_case`` for Rust idioms."""
    s1 = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", name)
    return re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", s1).lower()


# ----------------------------------------------------------------------
# Tokenization helpers
# ----------------------------------------------------------------------

# Match a quoted string literal, a /* ... */ block comment, or a #/// line
# comment.  We rely on alternation order (string first) so that ``#`` and
# ``//`` *inside* a quoted string are preserved -- e.g. ``format "Error #42"``
# or ``format "see https://..."`` would otherwise have their closing ``"``
# eaten by a naive line-comment regex, leaving an unbalanced quote that
# downstream brace-matching would skip past.
_STRING_OR_COMMENT_RE = re.compile(
    r"""
    "(?:\\.|[^"\\])*"           # double-quoted string (with escapes)
    | '(?:\\.|[^'\\])*'         # single-quoted string (with escapes)
    | (?s:/\*.*?\*/)            # /* ... */ block comment (DOTALL-scoped)
    | \#[^\n]*                  # # line comment (stops at newline)
    | //[^\n]*                  # // line comment (stops at newline)
    """,
    re.VERBOSE,
)


def _strip_comments_and_block_comments(text: str) -> str:
    """Strip ``//`` / ``#`` line comments and ``/* */`` block comments.

    String literals are *preserved* verbatim so that ``#`` or ``//`` inside a
    quoted FPP value (most commonly an event/format string) are not mistaken
    for the start of a comment.  Annotation lines starting with ``@`` are not
    matched by this regex and therefore pass through unchanged.
    """

    def _sub(m: "re.Match[str]") -> str:
        text_match = m.group(0)
        if text_match.startswith(('"', "'")):
            return text_match  # keep string literals intact
        return ""  # drop the comment

    return _STRING_OR_COMMENT_RE.sub(_sub, text)


def _strip_comments(text: str) -> str:
    """Remove ``//`` and ``#`` line comments while preserving string literals."""
    return _strip_comments_and_block_comments(text)


def _strip_block_comments(text: str) -> str:
    """Remove ``/* ... */`` block comments while preserving string literals.

    Retained as a separately-named entry point for readability at call sites;
    the underlying scanner already covers both line and block comments in one
    pass, so calling :func:`_strip_comments` first makes this idempotent.
    """
    return _strip_comments_and_block_comments(text)


def _find_matching_brace(text: str, start_idx: int) -> int:
    """Given the index of an opening ``{``, return the index of its matching ``}``.

    Honors single- and double-quoted strings so that braces inside string
    literals are not counted.  Raises ``ValueError`` if no match is found.
    """
    depth = 0
    i = start_idx
    n = len(text)
    while i < n:
        ch = text[i]
        if ch == '"' or ch == "'":
            # Skip the string literal
            quote = ch
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\":
                    i += 2
                    continue
                i += 1
            i += 1
            continue
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise ValueError("Unmatched '{' in FPP source")


def _split_top_level_commas(text: str) -> List[str]:
    """Split a parenthesized argument list on top-level commas.

    The FPP arg lists are line-broken with backslash continuations, so callers
    should pre-flatten newlines.  This helper only handles parenthesis nesting.
    """
    parts: List[str] = []
    depth = 0
    last = 0
    for i, ch in enumerate(text):
        if ch == "(" or ch == "[":
            depth += 1
        elif ch == ")" or ch == "]":
            depth -= 1
        elif ch == "," and depth == 0:
            parts.append(text[last:i])
            last = i + 1
    if text[last:].strip():
        parts.append(text[last:])
    return parts


# ----------------------------------------------------------------------
# Argument-list parser
# ----------------------------------------------------------------------

_ARG_RE = re.compile(
    r"""
    ^\s*\$?(?P<name>[A-Za-z_][A-Za-z0-9_]*)   # arg name, may be $-escaped
    \s*:\s*
    (?P<type>[A-Za-z_][\w.]*)                  # FPP type (e.g. U32, Ref.Foo)
    (?:\s+format\s+\"[^\"]*\")?                # optional format spec
    \s*$
    """,
    re.VERBOSE,
)


def _parse_args(arg_text: str) -> List[FppArg]:
    """Parse the contents of ``( ... )`` into a list of ``FppArg``.

    FPP separates formal arguments with either commas or newlines; both
    forms are handled here.  Backslash line-continuations and spurious
    whitespace are normalized first.
    """
    # Remove backslash line-continuations.
    flat = re.sub(r"\\\s*\n", "\n", arg_text)
    if not flat.strip():
        return []

    # Split on commas at the top level first (so that comma-separated lists
    # work even when they are spread across multiple physical lines).  Then,
    # for each chunk, additionally split on newlines because FPP allows
    # newline-only separators inside argument lists.
    raw_chunks: List[str] = []
    for piece in _split_top_level_commas(flat):
        for line in piece.splitlines():
            line = line.strip()
            if line:
                raw_chunks.append(line)

    args: List[FppArg] = []
    for chunk in raw_chunks:
        m = _ARG_RE.match(chunk)
        if not m:
            # Skip arguments we cannot parse - this leaves room for richer
            # FPP types without breaking the autocoder; it just means the
            # corresponding handler will not get this argument forwarded.
            continue
        args.append(FppArg(name=m.group("name"), fpp_type=m.group("type")))
    return args


# ----------------------------------------------------------------------
# Component body parser
# ----------------------------------------------------------------------

_COMPONENT_HEADER_RE = re.compile(
    r"""
    (?P<annot>(?:^[ \t]*@[^\n]*\n)+)?            # optional annotation block
    [ \t]*(?P<kind>active|passive|queued)\s+component\s+
    (?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\{
    """,
    re.VERBOSE | re.MULTILINE,
)

_COMMAND_RE = re.compile(
    r"""
    \b(?P<kind>sync|async|guarded)\s+command\s+
    (?P<name>[A-Za-z_][A-Za-z0-9_]*)
    (?:\s*\((?P<args>[^)]*)\))?
    """,
    re.VERBOSE,
)

_EVENT_RE = re.compile(
    r"""
    \bevent\s+
    (?P<name>[A-Za-z_][A-Za-z0-9_]*)
    (?:\s*\((?P<args>[^)]*)\))?
    (?P<tail>[^\n]*)
    """,
    re.VERBOSE,
)

# FPP severities: ``activity low | activity high | warning low | warning high
# | command | fatal | diagnostic``.  We capture the *two-word* forms first so
# that "activity low" doesn't get partially matched as just "activity".
_SEVERITY_RE = re.compile(
    r"""\bseverity\s+
    (?P<sev>
        activity\s+low
      | activity\s+high
      | warning\s+low
      | warning\s+high
      | command
      | fatal
      | diagnostic
    )
    """,
    re.VERBOSE,
)


# Map FPP severity to the F Prime ``log_<SEVERITY>_<Name>`` infix used in
# the auto-coded base class.
_SEVERITY_TO_LOG_INFIX = {
    "activity_low": "ACTIVITY_LO",
    "activity_high": "ACTIVITY_HI",
    "warning_low": "WARNING_LO",
    "warning_high": "WARNING_HI",
    "command": "COMMAND",
    "fatal": "FATAL",
    "diagnostic": "DIAGNOSTIC",
}


def severity_to_log_infix(severity: str) -> str:
    """Translate a parsed severity (snake_case) to its log-method infix.

    Unknown values fall back to ``ACTIVITY_LO`` to keep the autocoder
    forward-compatible with future FPP severity additions.
    """
    return _SEVERITY_TO_LOG_INFIX.get(severity, "ACTIVITY_LO")


def _parse_severity(tail: str) -> str:
    """Extract the severity from the tail of an ``event`` declaration.

    Defaults to ``activity_low`` when the declaration omits an explicit
    severity, matching FPP's default.
    """
    if not tail:
        return "activity_low"
    m = _SEVERITY_RE.search(tail)
    if not m:
        return "activity_low"
    raw = m.group("sev").lower()
    return re.sub(r"\s+", "_", raw)

_TELEMETRY_RE = re.compile(
    r"\btelemetry\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*:\s*(?P<type>[A-Za-z_][\w.]*)"
)

_PARAMETER_RE = re.compile(
    r"\bparam\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*:\s*(?P<type>[A-Za-z_][\w.]*)"
)


def _component_is_rust_annotated(annotation_block: Optional[str]) -> bool:
    if not annotation_block:
        return False
    for raw in annotation_block.splitlines():
        line = raw.strip()
        if not line.startswith("@"):
            continue
        body = line.lstrip("@").strip()
        if body == FPRIME_RUST_ANNOTATION:
            return True
    return False


def _strip_annotation_lines(body: str) -> str:
    """Remove lines starting with ``@`` from a block of FPP.

    FPP annotations attach to the following declaration but otherwise contain
    free-form text.  The autocoder only cares about the structured kinds
    (``command``, ``event``, ``telemetry``, ``param``); stripping annotation
    lines avoids regex false-positives on words like ``event`` or ``command``
    that show up in user-authored doc comments.
    """
    return "\n".join(
        line for line in body.splitlines() if not line.lstrip().startswith("@")
    )


def _parse_component_body(
    body: str,
) -> Tuple[List[FppCommand], List[FppEvent], List[FppTelemetry], List[FppParameter]]:
    """Extract commands, events, telemetry, parameters from a component body."""
    body = _strip_annotation_lines(body)
    # Normalize FPP's ``\`` line-continuation syntax so that a declaration
    # spread across several physical lines becomes one logical line.  Each
    # continuation is replaced with a single space to keep lexical
    # boundaries intact for the following regex passes.
    body = re.sub(r"\\\s*\n", " ", body)
    commands: List[FppCommand] = []
    for m in _COMMAND_RE.finditer(body):
        commands.append(
            FppCommand(
                name=m.group("name"),
                kind=m.group("kind"),
                args=_parse_args(m.group("args") or ""),
            )
        )

    events: List[FppEvent] = []
    for m in _EVENT_RE.finditer(body):
        events.append(
            FppEvent(
                name=m.group("name"),
                args=_parse_args(m.group("args") or ""),
                severity=_parse_severity(m.group("tail") or ""),
            )
        )

    telemetry: List[FppTelemetry] = []
    for m in _TELEMETRY_RE.finditer(body):
        telemetry.append(FppTelemetry(name=m.group("name"), fpp_type=m.group("type")))

    parameters: List[FppParameter] = []
    for m in _PARAMETER_RE.finditer(body):
        parameters.append(FppParameter(name=m.group("name"), fpp_type=m.group("type")))

    return commands, events, telemetry, parameters


# ----------------------------------------------------------------------
# Module-level walker
# ----------------------------------------------------------------------

_MODULE_RE = re.compile(
    r"\bmodule\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\{",
    re.MULTILINE,
)


def _module_inner_text(text: str) -> str:
    """Return ``text`` with every nested ``module ... { ... }`` body blanked out.

    The braces and the module header are preserved (so byte-offsets are not
    perturbed for downstream regex matches) but the body is replaced with
    spaces.  This lets us scan a single module level without having matches
    bleed into nested modules.
    """
    out = list(text)
    pos = 0
    while pos < len(out):
        m = _MODULE_RE.search(text, pos)
        if not m:
            break
        try:
            body_end = _find_matching_brace(text, m.end() - 1)
        except ValueError:
            break
        # Blank out the body but keep the braces so brace-counting is valid.
        for i in range(m.end(), body_end):
            if out[i] != "\n":
                out[i] = " "
        pos = body_end + 1
    return "".join(out)


def _walk_modules(text: str, namespace: List[str]) -> List[Tuple[List[str], str]]:
    """Return ``(namespace, slice_text)`` pairs for every module level.

    The returned slices have nested module bodies blanked out so callers can
    safely apply a top-level regex (e.g. ``component`` declarations) without
    matching declarations from a nested module.
    """
    results: List[Tuple[List[str], str]] = [(list(namespace), _module_inner_text(text))]
    pos = 0
    while pos < len(text):
        m = _MODULE_RE.search(text, pos)
        if not m:
            break
        try:
            body_end = _find_matching_brace(text, m.end() - 1)
        except ValueError:
            break
        body_start = m.end()
        inner = text[body_start:body_end]
        sub_ns = namespace + [m.group("name")]
        results.extend(_walk_modules(inner, sub_ns))
        pos = body_end + 1
    return results


def parse_fpp_file(path: Path) -> List[FppComponent]:
    """Return all ``@ fprime-rust`` annotated components in ``path``.

    Components defined in unannotated components are silently skipped, matching
    the Python autocoder's behavior.
    """
    text = path.read_text(encoding="utf-8")
    text = _strip_block_comments(text)
    text = _strip_comments(text)

    components: List[FppComponent] = []
    for namespace, slice_ in _walk_modules(text, []):
        for header in _COMPONENT_HEADER_RE.finditer(slice_):
            try:
                body_end = _find_matching_brace(slice_, header.end() - 1)
            except ValueError:
                continue
            body = slice_[header.end() : body_end]
            if not _component_is_rust_annotated(header.group("annot")):
                continue
            cmds, evs, tlm, prm = _parse_component_body(body)
            components.append(
                FppComponent(
                    name=header.group("name"),
                    kind=header.group("kind"),
                    namespace=list(namespace),
                    commands=cmds,
                    events=evs,
                    telemetry=tlm,
                    parameters=prm,
                    source_file=path,
                )
            )
    return components
