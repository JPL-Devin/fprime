####
# NavSensor Component
#
# Queued component that reads navigation/IMU sensor data
# and provides attitude and rate telemetry.
####

set(SOURCE_FILES
  "${CMAKE_CURRENT_LIST_DIR}/NavSensor.fpp"
  "${CMAKE_CURRENT_LIST_DIR}/NavSensor.cpp"
)
set(HEADER_FILES
  "${CMAKE_CURRENT_LIST_DIR}/NavSensor.hpp"
)

register_fprime_module()
