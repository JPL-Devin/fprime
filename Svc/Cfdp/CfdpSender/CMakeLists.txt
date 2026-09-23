####
# F prime CMakeLists.txt:
#
# SOURCE_FILES: combined list of source and autocoding files
# MOD_DEPS: (optional) module dependencies
####

set(SOURCE_FILES
  "${CMAKE_CURRENT_LIST_DIR}/CfdpSender.fpp"
  "${CMAKE_CURRENT_LIST_DIR}/CfdpSender.cpp"
)
set(MOD_DEPS
  Os
  Fw/Buffer
  CFDP/Checksum
  Svc/Cfdp/Types
)
register_fprime_module()

### UTs ###
set(UT_SOURCE_FILES
  "${CMAKE_CURRENT_LIST_DIR}/CfdpSender.fpp"
  "${CMAKE_CURRENT_LIST_DIR}/test/ut/CfdpSenderTester.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/test/ut/CfdpSenderTestMain.cpp"
)
set(UT_MOD_DEPS
  Os
  Fw/Buffer
  CFDP/Checksum
  Svc/Cfdp/Types
)
register_fprime_ut()
