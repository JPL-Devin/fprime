####
# FprimeRust CMakeLists.txt
#
# Registers the FprimeRust support library at the root of the F Prime build
# graph.  It currently exposes no C++ sources of its own (the runtime is
# pure-Rust), but is kept as a registered F Prime library so that downstream
# deployments can pick up the autocoder hooks via ``library_locations``.
####
register_fprime_library(
        FprimeRust
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/FprimeRust.hpp"
    DEPENDS
        Fw_Types
)
