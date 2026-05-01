####
# autocoder/fprime_rust.cmake:
#
# Implements the fprime-rust autocoder.  For every module that contains an
# FPP file with a component annotated ``@ fprime-rust`` this autocoder
# expands the model into:
#
#   - ``<Component>RustImpl.hpp`` / ``<Component>RustImpl.cpp`` -- the C++
#     shim that derives from the FPP-generated component base and forwards
#     handlers to Rust via an extern "C" FFI.
#   - ``<component>_base.rs`` -- the Rust trait/FFI surface the user crate
#     depends on (regenerated on every build).
#   - ``<Component>.template.rs`` -- a one-time-generated template the user
#     copies / renames to ``<Component>.rs`` and fills in.
#
# CMake invokes ``fprime-rust-ac bindings ... --dry-run`` first to discover
# the file list, then attaches a custom command that materialises the files.
# This pattern matches ``fprime-python``'s autocoder integration.
####
include_guard()
include(utilities)
include(autocoder/helpers)

autocoder_setup_for_multiple_sources()

####
# Function `fprime_rust_is_supported`:
# Mark every ``.fpp`` file as a candidate input.  The autocoder itself is
# responsible for filtering down to ``@ fprime-rust`` annotated components.
####
function(fprime_rust_is_supported AC_INPUT_FILE)
    autocoder_support_by_suffix(".fpp" "${AC_INPUT_FILE}" TRUE)
endfunction(fprime_rust_is_supported)

####
# Function `fprime_rust_setup_autocode`:
# Discover and emit the Rust + C++ artifacts for a single module.
####
function(fprime_rust_setup_autocode MODULE_NAME AC_INPUT_FILES)
    find_program(FPRIME_RUST_AC NAMES fprime-rust-ac REQUIRED)
    if (FPRIME_RUST_AC MATCHES ".*-NOTFOUND")
        message(FATAL_ERROR "[fprime-rust] 'fprime-rust-ac' not found on PATH; install the fprime-rust Python package")
    endif()

    # Step 1: dry run to learn the file set.
    execute_process(
        COMMAND "${FPRIME_RUST_AC}"
            "bindings"
            "--dry-run"
            "--output-directory" "${CMAKE_CURRENT_BINARY_DIR}"
            "--translation-units" ${AC_INPUT_FILES}
        OUTPUT_VARIABLE GENERATED_RAW
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
    )
    string(REPLACE " " ";" GENERATED_LIST "${GENERATED_RAW}")
    string(REPLACE "\n" "" GENERATED_LIST "${GENERATED_LIST}")

    # Skip modules that produced no files (i.e. no annotated components).
    get_target_property(MODULE_TYPE ${MODULE_NAME} TYPE)
    if (NOT GENERATED_LIST OR MODULE_TYPE STREQUAL "INTERFACE_LIBRARY")
        set(AUTOCODER_DEPENDENCIES "" PARENT_SCOPE)
        set(AUTOCODER_GENERATED_BUILD_SOURCES "" PARENT_SCOPE)
        set(AUTOCODER_GENERATED_OTHER "" PARENT_SCOPE)
        return()
    endif()

    # Bin the generated files by extension.
    set(GENERATED_HPP_FILES "${GENERATED_LIST}")
    set(GENERATED_CPP_FILES "${GENERATED_LIST}")
    set(GENERATED_RUST_BASE_FILES "${GENERATED_LIST}")
    set(GENERATED_RUST_TEMPLATE_FILES "${GENERATED_LIST}")
    # The C++ shim is emitted as ``<Component>.hpp`` / ``<Component>.cpp``
    # so the FPP-generated topology can ``#include`` it under the standard
    # ``Path/Component.hpp`` filename without any path massaging.  Filter
    # *out* the FPP-emitted ``<Component>ComponentAc.hpp`` (which the FPP
    # autocoder writes -- not us) by anchoring on ``\.hpp$`` while
    # excluding any path containing ``ComponentAc``.
    list(FILTER GENERATED_HPP_FILES INCLUDE REGEX "\\.hpp$")
    list(FILTER GENERATED_HPP_FILES EXCLUDE REGEX "ComponentAc\\.hpp$")
    list(FILTER GENERATED_CPP_FILES INCLUDE REGEX "\\.cpp$")
    list(FILTER GENERATED_CPP_FILES EXCLUDE REGEX "ComponentAc\\.cpp$")
    list(FILTER GENERATED_RUST_BASE_FILES INCLUDE REGEX "_base\\.rs$")
    list(FILTER GENERATED_RUST_TEMPLATE_FILES INCLUDE REGEX "\\.template\\.rs$")

    # Step 2: real run, generating the files.
    add_custom_command(
        OUTPUT ${GENERATED_HPP_FILES} ${GENERATED_CPP_FILES}
               ${GENERATED_RUST_BASE_FILES} ${GENERATED_RUST_TEMPLATE_FILES}
        DEPENDS ${AC_INPUT_FILES}
        COMMAND "${FPRIME_RUST_AC}"
            "bindings"
            "--output-directory" "${CMAKE_CURRENT_BINARY_DIR}"
            "--translation-units" ${AC_INPUT_FILES}
        COMMENT "[fprime-rust] regenerating bindings for ${MODULE_NAME}"
    )

    append_list_property("${GENERATED_RUST_BASE_FILES}" TARGET "${MODULE_NAME}"
        PROPERTY FPRIME_RUST_GENERATED_BASE_RS_FILES)
    append_list_property("${GENERATED_RUST_TEMPLATE_FILES}" TARGET "${MODULE_NAME}"
        PROPERTY FPRIME_RUST_GENERATED_TEMPLATE_RS_FILES)
    append_list_property("${GENERATED_HPP_FILES}" TARGET "${MODULE_NAME}"
        PROPERTY FPRIME_RUST_GENERATED_HPP_FILES)
    append_list_property("${AC_INPUT_FILES}" TARGET "${MODULE_NAME}"
        PROPERTY FPRIME_RUST_INPUT_FPP_FILES)

    set(AUTOCODER_DEPENDENCIES "" PARENT_SCOPE)
    set(AUTOCODER_GENERATED_BUILD_SOURCES ${GENERATED_CPP_FILES} PARENT_SCOPE)
    set(AUTOCODER_GENERATED_OTHER ${GENERATED_HPP_FILES}
        ${GENERATED_RUST_BASE_FILES} ${GENERATED_RUST_TEMPLATE_FILES} PARENT_SCOPE)
endfunction(fprime_rust_setup_autocode)
