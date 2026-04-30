####
# target/fprime_rust.cmake:
#
# F Prime build target that connects the fprime-rust autocoder to the
# Cargo-driven Rust build for a module.  Mirrors the structure of
# ``target/fprime_python.cmake``.
#
# Per module the steps are:
#  1. Run the autocoder (via ``run_ac_set``) to populate the build cache with
#     ``<Component>RustImpl.{hpp,cpp}`` and ``<component>_base.rs``.
#  2. If the autocoder produced any Rust output, materialize a Cargo
#     workspace under ``<binary_dir>/_fprime_rust_crate`` and invoke
#     ``cargo build --release`` against it.  The resulting ``.a`` is added to
#     the C++ module's link line.
#  3. Copy any newly-generated ``<Component>.template.rs`` into the source
#     tree if the user has not yet committed a hand-written ``<Component>.rs``.
####
include_guard()
include(autocoder/autocoder)

# Capture this file's directory at *include* time.  ``CMAKE_CURRENT_LIST_DIR``
# inside a function body resolves to the *caller's* listfile directory, which
# during per-module processing is the user component's source directory --
# definitely not where the runtime crate lives.  Stash the value in a cache
# variable so every later function reference resolves to ``fprime-rust/cmake/
# target/`` regardless of where the function is invoked from.
set(_FPRIME_RUST_TARGET_DIR "${CMAKE_CURRENT_LIST_DIR}"
    CACHE INTERNAL "Directory containing fprime-rust target/fprime_rust.cmake"
)

####
# Function `fprime_rust_add_global_target`:
# No-op global registration; per-module integration is done in
# ``fprime_rust_add_module_target``.
####
function(fprime_rust_add_global_target)
endfunction()

####
# Function `_fprime_rust_invoke_crate`:
# Helper that runs ``fprime-rust-ac crate`` to lay down a Cargo workspace
# for a module and adds custom commands to compile it via ``cargo``.
####
function(_fprime_rust_invoke_crate MODULE)
    get_target_property(BASE_FILES "${MODULE}" FPRIME_RUST_GENERATED_BASE_RS_FILES)
    get_target_property(INPUT_FPP "${MODULE}" FPRIME_RUST_INPUT_FPP_FILES)
    if (NOT BASE_FILES)
        return()
    endif()
    find_program(FPRIME_RUST_AC NAMES fprime-rust-ac REQUIRED)
    find_program(FPRIME_RUST_CARGO NAMES cargo)
    if (NOT FPRIME_RUST_CARGO OR FPRIME_RUST_CARGO MATCHES ".*-NOTFOUND")
        message(WARNING "[fprime-rust] cargo not found on PATH; the Rust crate for ${MODULE} will not be built")
        return()
    endif()

    set(CRATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_fprime_rust_crate")
    set(USER_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(RUNTIME_DIR "${FPRIME_RUST_RUNTIME_DIR}")
    if (NOT RUNTIME_DIR)
        # Default: resolve the runtime crate relative to *this* file's
        # directory (captured at include time as ``_FPRIME_RUST_TARGET_DIR``).
        # Using ``CMAKE_CURRENT_LIST_DIR`` here would point at the calling
        # listfile -- typically the module's own source directory.
        set(RUNTIME_DIR "${_FPRIME_RUST_TARGET_DIR}/../../runtime")
    endif()
    # Normalise so that paths fed to ``cargo`` / Cargo.toml don't carry
    # ``..`` segments that depend on the working directory at build time.
    get_filename_component(RUNTIME_DIR "${RUNTIME_DIR}" REALPATH)

    # Generate the Cargo workspace files.
    execute_process(
        COMMAND "${FPRIME_RUST_AC}"
            "crate"
            "--translation-units" ${INPUT_FPP}
            "--output-directory" "${CRATE_DIR}"
            "--bindings-dir" "${CMAKE_CURRENT_BINARY_DIR}"
            "--user-dir" "${USER_DIR}"
            "--runtime-path" "${RUNTIME_DIR}"
        COMMAND_ERROR_IS_FATAL ANY
    )

    # Add a build step per crate directory, looking for staticlibs to link.
    file(GLOB CRATE_SUBDIRS LIST_DIRECTORIES true "${CRATE_DIR}/*_crate")
    foreach (SUBDIR IN LISTS CRATE_SUBDIRS)
        get_filename_component(CRATE_NAME "${SUBDIR}" NAME)
        string(REGEX REPLACE "_crate$" "" CRATE_NAME "${CRATE_NAME}")
        set(LIB_PATH "${SUBDIR}/target/release/libfprime_rust_${CRATE_NAME}.a")
        add_custom_command(
            OUTPUT  "${LIB_PATH}"
            DEPENDS ${BASE_FILES}
            COMMAND "${FPRIME_RUST_CARGO}" build --release
            WORKING_DIRECTORY "${SUBDIR}"
            COMMENT "[fprime-rust] cargo build for ${CRATE_NAME}"
        )
        add_custom_target("${MODULE}_${CRATE_NAME}_rust" DEPENDS "${LIB_PATH}")
        add_dependencies("${MODULE}" "${MODULE}_${CRATE_NAME}_rust")
        target_link_libraries("${MODULE}" PRIVATE "${LIB_PATH}")
    endforeach()
endfunction()

####
# Function `_fprime_rust_install_template`:
# Copy any newly generated ``<Component>.template.rs`` files into the source
# directory.  The copy is conditional: if the user has already produced a
# matching ``<Component>.rs`` we leave it alone.
####
function(_fprime_rust_install_template MODULE)
    get_target_property(TEMPLATE_FILES "${MODULE}" FPRIME_RUST_GENERATED_TEMPLATE_RS_FILES)
    if (NOT TEMPLATE_FILES)
        return()
    endif()
    foreach (TEMPLATE IN LISTS TEMPLATE_FILES)
        get_filename_component(TEMPLATE_NAME "${TEMPLATE}" NAME)
        string(REGEX REPLACE "\\.template\\.rs$" ".rs" REAL_NAME "${TEMPLATE_NAME}")
        set(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${REAL_NAME}")
        if (NOT EXISTS "${REAL_PATH}")
            add_custom_command(TARGET "${MODULE}" POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy "${TEMPLATE}" "${REAL_PATH}"
                COMMENT "[fprime-rust] seeding ${REAL_NAME} from template"
            )
        endif()
    endforeach()
endfunction()

####
# Function `fprime_rust_add_module_target`:
# Run the autocoder, materialise the Cargo crate, and link in the Rust
# staticlib for a module.
####
function(fprime_rust_add_module_target MODULE TARGET SOURCE_FILES DEPENDENCIES)
    run_ac_set("${MODULE}" "autocoder/fprime_rust")
    _fprime_rust_invoke_crate("${MODULE}")
    _fprime_rust_install_template("${MODULE}")
endfunction(fprime_rust_add_module_target)

####
# Function `fprime_rust_add_deployment_target`:
# Deployment-level integration is currently identical to module-level: link
# the per-module Rust staticlibs into the deployment via standard CMake
# transitive dependencies.
####
function(fprime_rust_add_deployment_target MODULE TARGET SOURCES DEPENDENCIES FULL_DEPENDENCIES)
    fprime_rust_add_module_target("${MODULE}" "${TARGET}" "${SOURCES}" "${DEPENDENCIES}")
endfunction()
