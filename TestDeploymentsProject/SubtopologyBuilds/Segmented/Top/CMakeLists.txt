####
# Segmented topology module. Exactly one topology definition is autocoded per build: topology.fpp
# (ComCcsds.SegmentedSubtopology) or topologySdls.fpp (ComCcsdsSdls.SegmentedSubtopology with the
# sdlsKeyManager instance from instancesSdls.fpp), selected by the SEGMENTED_SDLS option.
####
if (SEGMENTED_SDLS)
    set(SEGMENTED_TOPOLOGY_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/instancesSdls.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/topologySdls.fpp"
    )
    set(SEGMENTED_TOPOLOGY_DEPENDS
        Svc_Ccsds_AesGcmDecryptor
        Svc_Ccsds_ClearTextEncryptor
        Svc_Ccsds_SdlsFileKeyManager
    )
else()
    set(SEGMENTED_TOPOLOGY_INPUTS "${CMAKE_CURRENT_LIST_DIR}/topology.fpp")
    set(SEGMENTED_TOPOLOGY_DEPENDS)
endif()

register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/instances.fpp"
        ${SEGMENTED_TOPOLOGY_INPUTS}
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/SegmentedTopology.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/SegmentedTopology.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/SegmentedTopologyDefs.hpp"
    DEPENDS
        Fw_Logger
        Svc_PosixTime
        ${SEGMENTED_TOPOLOGY_DEPENDS}
)
