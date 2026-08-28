####
# PowerManager Component
#
# Passive component that monitors spacecraft power state,
# battery voltage, and power bus status.
####

set(SOURCE_FILES
  "${CMAKE_CURRENT_LIST_DIR}/PowerManager.fpp"
  "${CMAKE_CURRENT_LIST_DIR}/PowerManager.cpp"
)
set(HEADER_FILES
  "${CMAKE_CURRENT_LIST_DIR}/PowerManager.hpp"
)

register_fprime_module()
