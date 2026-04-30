####
# library.cmake:
#
# Entry point for the `fprime-rust` library, mirroring the convention used by other F Prime libraries
# (e.g. `fprime-python`). When a deployment lists this directory under `library_locations` in its
# `settings.ini`, this file is automatically included by the F Prime CMake build system.
####
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/FprimeRust")
