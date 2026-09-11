####
# 'Segmented' Deployment:
#
# Minimal test deployment that instantiates the TC Segment Header / MAP uplink variants of the
# ComCcsds and ComCcsdsSdls subtopologies (`SegmentedSubtopology`) so that both are run through
# fpp-to-cpp and compiled in CI. `SEGMENTED_SDLS=ON` selects the SDLS variant, which overrides
# the ComCcsdsSdls decryptor with Svc.Ccsds.AesGcmDecryptor (requires OpenSSL >= 3.5; pass
# -DOPENSSL_ROOT_DIR if it is not the system OpenSSL).
####

option(SEGMENTED_SDLS "Build the SDLS variant (ComCcsdsSdls.SegmentedSubtopology + AesGcmDecryptor)" OFF)

if (SEGMENTED_SDLS)
    # Svc/Ccsds/AesGcmDecryptor is only registered when OpenSSL >= 3.5 is found; a clear-text
    # decryptor would silently void the authentication property, so fail the configure instead.
    if (NOT TARGET Svc_Ccsds_AesGcmDecryptor)
        message(FATAL_ERROR
            "SEGMENTED_SDLS=ON requires Svc_Ccsds_AesGcmDecryptor, which is not registered: "
            "OpenSSL >= 3.5 was not found. Pass -DOPENSSL_ROOT_DIR=<openssl-3.5 prefix>.")
    endif()
    # The imported OpenSSL::Crypto target is directory-scoped; import it here so the
    # deployment's transitive link check can resolve it.
    find_package(OpenSSL 3.5 REQUIRED)
    add_compile_definitions(SEGMENTED_SDLS=1)
    add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/config/")
endif()
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Top/")

# Module names derive from the path below the project root: the deployment target is
# SubtopologyBuilds_Segmented (what `fprime-util build` in this directory builds) and the
# topology module is SubtopologyBuilds_Segmented_Top.
register_fprime_deployment(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Main.cpp"
    DEPENDS
        SubtopologyBuilds_Segmented_Top
)
