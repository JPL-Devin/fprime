####
# F prime CMakeLists.txt:
#
# SOURCE_FILES: combined list of source and autocoding files
# MOD_DEPS: (optional) module dependencies
####
add_compile_options(
  -Wno-shadow
)

set(_REF_TOP_INSTANCES_FPP "${CMAKE_CURRENT_LIST_DIR}/instances.fpp")
set(_REF_TOP_TOPOLOGY_FPP  "${CMAKE_CURRENT_LIST_DIR}/topology.fpp")
set(_REF_TOP_DEPENDS
    Fw_Logger
    Svc_PosixTime
    Svc_TlmPacketizer_config_TlmPacketizerConfig # Supports packet autocoding w/o TlmPacketizer
)

####
# Optional: splice the Rust-backed reference component into the topology.
#
# When ``-DFPRIME_RUST_ENABLE=ON`` is set, this block reads ``instances.fpp``
# and ``topology.fpp`` verbatim, appends a single ``instance rustExample``
# line just before the closing brace of the surrounding block, writes the
# result into ``${CMAKE_CURRENT_BINARY_DIR}``, and registers the rewritten
# files as the autocoder inputs in place of the originals.  Splicing is
# preferred over a separate ``rustInstance.fpp`` import because FPP does not
# allow a topology declaration to be re-opened across files.
#
# When ``OFF`` (the default) the originals are used verbatim and the build
# is bit-identical to upstream.
####
if (FPRIME_RUST_ENABLE)
    # Use literal newline + literal backslash characters that survive CMake's
    # quoted-argument processing.  ``string(ASCII 10 ...)`` returns a single
    # ``\n`` byte and ``string(ASCII 92 ...)`` returns ``\``; combining them
    # this way side-steps the fact that ``string(REGEX REPLACE)`` does not
    # interpret backslash escapes in the replacement string.
    string(ASCII 10 NL)
    string(ASCII 92 BS)
    set(_RUST_INSTANCE_DECL
        "  instance rustExample: Ref.RustExample base id 0x10016000 ${BS}${NL}    queue size Default.QUEUE_SIZE ${BS}${NL}    stack size Default.STACK_SIZE ${BS}${NL}    priority 18${NL}"
    )
    set(_RUST_TOPOLOGY_REF "    instance rustExample${NL}")

    # Splice: read the source file, locate the *last* ``\n}\n`` (the
    # closing brace of the outermost ``module`` / ``topology`` block) and
    # insert the extra declaration immediately before it.  Done with a
    # plain ``string(FIND ...)`` instead of REGEX REPLACE so we don't have
    # to fight CMake's regex escape rules.
    file(READ "${_REF_TOP_INSTANCES_FPP}" _instances_orig)
    string(FIND "${_instances_orig}" "${NL}}" _close_pos REVERSE)
    if (_close_pos LESS 0)
        message(FATAL_ERROR "Could not find closing brace in ${_REF_TOP_INSTANCES_FPP}")
    endif()
    string(SUBSTRING "${_instances_orig}" 0 ${_close_pos} _before)
    math(EXPR _after_pos "${_close_pos} + 1")  # skip the leading \n
    string(SUBSTRING "${_instances_orig}" ${_after_pos} -1 _after)
    set(_REF_TOP_INSTANCES_FPP "${CMAKE_CURRENT_BINARY_DIR}/instances.fpp")
    file(WRITE "${_REF_TOP_INSTANCES_FPP}"
        "${_before}${NL}${_RUST_INSTANCE_DECL}${NL}${_after}"
    )

    # ``topology.fpp`` is processed identically except we splice into the
    # closing brace of ``topology Ref { ... }``: a two-space-indented
    # ``\n  }\n``.  Using FIND-REVERSE again locks onto that block since
    # the outer ``module Ref { ... }`` ends with ``\n}\n`` (single space).
    file(READ "${_REF_TOP_TOPOLOGY_FPP}" _topology_orig)
    string(FIND "${_topology_orig}" "${NL}  }" _topo_close_pos REVERSE)
    if (_topo_close_pos LESS 0)
        message(FATAL_ERROR "Could not find topology Ref closing brace in ${_REF_TOP_TOPOLOGY_FPP}")
    endif()
    string(SUBSTRING "${_topology_orig}" 0 ${_topo_close_pos} _topo_before)
    math(EXPR _topo_after_pos "${_topo_close_pos} + 1")
    string(SUBSTRING "${_topology_orig}" ${_topo_after_pos} -1 _topo_after)
    set(_REF_TOP_TOPOLOGY_FPP "${CMAKE_CURRENT_BINARY_DIR}/topology.fpp")
    file(WRITE "${_REF_TOP_TOPOLOGY_FPP}"
        "${_topo_before}${NL}${_RUST_TOPOLOGY_REF}${_topo_after}"
    )

    # ``topology.fpp`` includes ``RefPackets.fppi`` with a path relative
    # to the topology file's own directory.  Since we just relocated
    # ``topology.fpp`` into the build dir, also copy that include file
    # into the build dir AND inject a small extra packet covering the
    # rustExample telemetry channel (``Counter``) so that the
    # ``TlmPacketizer`` actually fans it out to the GDS rather than
    # silently dropping it.  We splice just before the closing brace of
    # ``packets RefPackets {`` (the line ``} omit {``).
    file(READ "${CMAKE_CURRENT_LIST_DIR}/RefPackets.fppi" _packets_orig)
    string(FIND "${_packets_orig}" "} omit {" _omit_pos REVERSE)
    if (_omit_pos LESS 0)
        message(FATAL_ERROR "Could not find ``} omit {`` block in RefPackets.fppi")
    endif()
    string(SUBSTRING "${_packets_orig}" 0 ${_omit_pos} _packets_before)
    string(SUBSTRING "${_packets_orig}" ${_omit_pos} -1 _packets_after)
    set(_RUST_PACKET
        "  packet RustExample id 38 group 2 {${NL}    rustExample.Counter${NL}  }${NL}${NL}"
    )
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/RefPackets.fppi"
        "${_packets_before}${_RUST_PACKET}${_packets_after}"
    )

    list(APPEND _REF_TOP_DEPENDS Ref_RustExample)
endif()

register_fprime_module(
  AUTOCODER_INPUTS
    "${_REF_TOP_INSTANCES_FPP}"
    "${_REF_TOP_TOPOLOGY_FPP}"
  SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/RefTopology.cpp"
  DEPENDS
    ${_REF_TOP_DEPENDS}
)
