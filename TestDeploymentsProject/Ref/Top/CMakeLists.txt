####
# F prime CMakeLists.txt:
#
# SOURCE_FILES: combined list of source and autocoding files
# MOD_DEPS: (optional) module dependencies
####
add_compile_options(
  -Wno-shadow
)

# Select the deployment topology file. Both define `deployment topology Ref` from the shared
# .fppi fragments; only the com-stack and telemetry-packet fragments differ.
option(REF_TC_SEGMENTED "Build Ref with the segmented TC uplink (ComCcsds.SegmentedSubtopology)" OFF)
if(REF_TC_SEGMENTED)
    set(REF_TOPOLOGY_FPP "${CMAKE_CURRENT_LIST_DIR}/topologySegmented.fpp")
else()
    set(REF_TOPOLOGY_FPP "${CMAKE_CURRENT_LIST_DIR}/topology.fpp")
endif()

register_fprime_module(
  AUTOCODER_INPUTS
    "${CMAKE_CURRENT_LIST_DIR}/instances.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/system.fpp"
    "${REF_TOPOLOGY_FPP}"
  SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/RefTopology.cpp"
  DEPENDS
    Fw_Logger
    Svc_PosixTime
    Svc_TlmPacketizer_config_TlmPacketizerConfig # Supports packet autocoding w/o TlmPacketizer  
)
