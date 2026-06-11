####
# project_interface.cmake:
#
# Defines the unified F Prime project interface target. This single INTERFACE library
# replaces both the global interface target (formerly in API.cmake) and the configuration
# interface target (formerly in config_assembler.cmake). It aggregates:
#
#   - Global include directories (source and binary locations)
#   - Build location properties (FPRIME_SOURCE_LOCATIONS, FPRIME_BINARY_LOCATIONS)
#   - Configuration module dependencies and chosen implementations
#
# Custom target properties:
#   FPRIME_SOURCE_LOCATIONS: list of source directories registered as build locations
#   FPRIME_BINARY_LOCATIONS: list of binary directories registered as build locations
#   FPRIME_CHOSEN_IMPLEMENTATIONS: list of chosen implementations from config modules
####
include_guard()

set(FPRIME_PROJECT_INTERFACE_TARGET "_fprime_project_interface" CACHE INTERNAL
    "Unified F Prime project interface target" FORCE)

if (NOT TARGET "${FPRIME_PROJECT_INTERFACE_TARGET}")
    add_library("${FPRIME_PROJECT_INTERFACE_TARGET}" INTERFACE)
endif()
