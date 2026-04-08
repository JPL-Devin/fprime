####
# F prime CMakeLists.txt:
#
# CFDP PDU types library
####

set(SOURCE_FILES
  "${CMAKE_CURRENT_LIST_DIR}/CfdpPdu.cpp"
)
set(MOD_DEPS
  Fw/Buffer
)
register_fprime_module()

### UTs ###
set(UT_SOURCE_FILES
  "${CMAKE_CURRENT_LIST_DIR}/CfdpPdu.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test/ut/CfdpPduTest.cpp"
)
set(UT_MOD_DEPS
  Fw/Buffer
)
register_fprime_ut()
