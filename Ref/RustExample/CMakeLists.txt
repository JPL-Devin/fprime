####
# Ref/RustExample CMakeLists.txt
#
# Registers the Rust-backed RustExample component.  The autocoder hooks
# defined under ``fprime-rust/cmake`` are responsible for emitting the C++
# shim and the per-component Cargo workspace; the user-authored
# ``RustExample.rs`` and ``RustExample.fpp`` are the only files committed
# to the source tree for this component.
####

# The C++ shim itself is emitted by the fprime-rust autocoder, so no SOURCES
# entry is required here.  Only the FPP and the user Rust file are listed.
register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/RustExample.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/RustExample.rs"
    DEPENDS
        Fw_Types
        Fw_Cmd
        Fw_Tlm
        Fw_Log
        Fw_Prm
        Fw_Time
)
