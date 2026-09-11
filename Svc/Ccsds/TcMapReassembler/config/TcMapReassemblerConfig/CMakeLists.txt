####
# Svc/Ccsds/TcMapReassembler/config/TcMapReassemblerConfig/CMakeLists.txt:
#
# Compile-time configuration for the TcMapReassembler component and its dedicated buffer pool.
# Projects override these constants by shadowing TcMapCfg.fpp in their own configuration module.
####
register_fprime_config(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/TcMapCfg.fpp"
)
