####
# fprime_install.cmake:
#
# Wrapper script invoked by POST_BUILD commands to install build artifacts.
# Defaults DESTDIR to FPRIME_INSTALL_DEST_DIR (passed via -D) when the user has
# not set DESTDIR in the environment. This keeps CMAKE_INSTALL_PREFIX at "/"
# so that DESTDIR alone controls the final install location.
#
# Expected -D arguments:
#   FPRIME_INSTALL_DEST_DIR  - default install directory (from configure step)
#   FPRIME_BUILD_DIR         - path to the CMake binary directory
#   CMAKE_INSTALL_COMPONENT  - install component to process
####
if(NOT DEFINED ENV{DESTDIR})
    set(ENV{DESTDIR} "${FPRIME_INSTALL_DEST_DIR}")
endif()
include("${FPRIME_BUILD_DIR}/cmake_install.cmake")
