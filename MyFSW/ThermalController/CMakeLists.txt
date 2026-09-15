####
# ThermalController Component
#
# Active component for spacecraft thermal management.
# Monitors temperature sensors and controls heaters with
# hysteresis-based automatic control.
####

set(SOURCE_FILES
  "${CMAKE_CURRENT_LIST_DIR}/ThermalController.fpp"
  "${CMAKE_CURRENT_LIST_DIR}/ThermalController.cpp"
)
set(HEADER_FILES
  "${CMAKE_CURRENT_LIST_DIR}/ThermalController.hpp"
)

register_fprime_module()
