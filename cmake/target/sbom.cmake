####
# cmake/target/sbom.cmake:
#
# A target used to add SBOM generation to the build. Will be invoked when running the "all" target
# and installed into the build_artifacts directory underneath the platform folder.
####
set(FPRIME__INTERNAL_SBOM_REDIRECTOR "${CMAKE_CURRENT_LIST_DIR}/tools/redirector.py" CACHE INTERNAL "Internal path to SBOM redirector script" FORCE)

####
# sbom_add_global_target:
# 
# Used to register a global target that will build with "all" and generates the SBOM.
#
#####
function(sbom_add_global_target TARGET)
    find_program(SYFT NAMES syft)
    # Check if syft is available before running
    if (SYFT)
        add_custom_target("${TARGET}" ALL
            COMMAND
            # Redirect to cleanly capture standard out
            ${PYTHON} ${FPRIME__INTERNAL_SBOM_REDIRECTOR} "${CMAKE_BINARY_DIR}/${PROJECT_NAME}_sbom.json"
            # syft arguments
            "${SYFT}" "dir:${FPRIME_PROJECT_ROOT}" -o spdx-json
            # Excludes .github paths not in the root of the project as those should not be activated by the project
            --exclude '*/**/.github'
            DEPENDS $<TARGET_PROPERTY:${TARGET},SBOM_DEPENDENCIES>
        )
        # Install the SBOM file
        install(FILES "${CMAKE_BINARY_DIR}/${PROJECT_NAME}_sbom.json" DESTINATION ${TOOLCHAIN_NAME} COMPONENT ${TARGET})
        add_custom_command(TARGET "${TARGET}" POST_BUILD COMMAND "${CMAKE_COMMAND}"
                          -DCMAKE_INSTALL_COMPONENT=${TARGET}
                          -DFPRIME_INSTALL_DEST=${FPRIME_INSTALL_DEST}
                          -DFPRIME_BUILD_DIR=${CMAKE_BINARY_DIR}
                          -P ${FPRIME_FRAMEWORK_PATH}/cmake/target/fprime_install.cmake)
    else()
        fprime_cmake_status("[INFO] Failed to find 'syft' on PATH, please install to generate software bill-of-materials")
    endif()
endfunction()

# Adds MODULE to the SBOM_DEPENDENCIES of the global SBOM target. Excluded modules are skipped: the SBOM target is
# part of `all`, so listing them would pull them back into `all`. With FPRIME_BUILD_ALL_INCLUDED_CODE=ON every module
# is listed, as before the option existed.
function(sbom__internal_add_dependency MODULE TARGET)
    if (TARGET "${TARGET}" AND TARGET "${MODULE}")
        get_target_property(FPRIME__INTERNAL_SBOM_TYPE "${MODULE}" TYPE)
        set(FPRIME__INTERNAL_SBOM_EXCLUDED FALSE)
        if (NOT FPRIME_BUILD_ALL_INCLUDED_CODE AND NOT FPRIME__INTERNAL_SBOM_TYPE STREQUAL "INTERFACE_LIBRARY")
            get_target_property(FPRIME__INTERNAL_SBOM_EXCLUDED "${MODULE}" EXCLUDE_FROM_ALL)
        endif()
        if (NOT FPRIME__INTERNAL_SBOM_EXCLUDED)
            append_list_property("${MODULE}" TARGET "${TARGET}" PROPERTY SBOM_DEPENDENCIES)
        endif()
    endif()
endfunction()

# For deployments
function(sbom_add_deployment_target MODULE TARGET SOURCES DEPENDENCIES FULL_DEPENDENCIES)
    sbom__internal_add_dependency("${MODULE}" "${TARGET}")
endfunction()

# Used to register all modules
function(sbom_add_module_target MODULE TARGET SOURCE_FILES DEPENDENCIES)
    sbom__internal_add_dependency("${MODULE}" "${TARGET}")
endfunction()
