####
# F Prime CMakeLists.txt:
#
# SOURCES: list of source files (to be compiled)
# AUTOCODER_INPUTS: list of files to be passed to the autocoders
# DEPENDS: list of libraries that this module depends on
#
# More information in the F´ CMake API documentation:
# https://fprime.jpl.nasa.gov/devel/docs/reference/api/cmake/API/
#
####

register_fprime_library(
  SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/SppZonePacker.cpp"
  AUTOCODER_INPUTS
    "${CMAKE_CURRENT_LIST_DIR}/SppZonePacker.fpp"
  DEPENDS
    Svc_Ccsds_Types
    Os
)

register_fprime_ut(
  SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/test/ut/SppZonePackerTestMain.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test/ut/SppZonePackerTester.cpp"
  AUTOCODER_INPUTS
    "${CMAKE_CURRENT_LIST_DIR}/SppZonePacker.fpp"
  DEPENDS
    Svc_Ccsds_Types
    STest
  UT_AUTO_HELPERS
)
