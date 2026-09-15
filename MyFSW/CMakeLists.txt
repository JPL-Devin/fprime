####
# 'MyFSW' Deployment:
#
# This sets up the build for the 'MyFSW' flight software application,
# including custom components. In addition, it imports FPrime.cmake,
# which includes the core F Prime components.
####

cmake_minimum_required(VERSION 3.26)
project(MyFSW VERSION 1.0.0 LANGUAGES C CXX)

# Find the fprime package and include the core codebase
set(FPRIME_INCLUDE_FRAMEWORK_CODE ON)
find_package(FPrime REQUIRED PATHS "${CMAKE_CURRENT_LIST_DIR}/..")

# Add subdirectories for the MyFSW project specific components
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/ThermalController/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/NavSensor/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/PowerManager/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Top/")

register_fprime_deployment(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Main.cpp"
    DEPENDS
        ${PROJECT_NAME}_Top
)
