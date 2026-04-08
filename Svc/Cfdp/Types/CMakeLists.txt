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
