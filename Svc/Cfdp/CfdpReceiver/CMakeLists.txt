####
# F prime CMakeLists.txt:
#
# SOURCE_FILES: combined list of source and autocoding files
# MOD_DEPS: (optional) module dependencies
####

set(SOURCE_FILES
  "${CMAKE_CURRENT_LIST_DIR}/CfdpReceiver.fpp"
  "${CMAKE_CURRENT_LIST_DIR}/CfdpReceiver.cpp"
)
set(MOD_DEPS
  Os
  Fw/Buffer
  CFDP/Checksum
  Svc/Cfdp/Types
)
register_fprime_module()
