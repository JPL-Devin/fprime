####
# 'TestDeploymentsProject' Project:
#
# This is the project that contains the set of test deployments. This includes:
# - The 'Ref' deployment, which is a historical reference for testing and demonstration
# - The 'SubtopologyBuilds/Segmented' deployment, a minimal build of the TC Segment Header / MAP
#   uplink variants of the ComCcsds and ComCcsdsSdls subtopologies (SEGMENTED_SDLS option)
####

# Allow CMake 3.0 - 4.0 projects to set the VERSION variable in the `project` call
cmake_policy(SET CMP0048 NEW)
# CMake basic setup: version consistent with requirements.txt, and define a project 
cmake_minimum_required(VERSION 3.26)
project(Ref VERSION 1.0.0 LANGUAGES C CXX)

# Find the fprime package and include the core codebase
set(FPRIME_INCLUDE_FRAMEWORK_CODE OFF)
find_package(FPrime REQUIRED PATHS "${CMAKE_CURRENT_LIST_DIR}/..")

# Hold this project to the same warning set F Prime validates against. The flags F Prime requires in
# order to build arrive with the framework targets themselves.
fprime_add_validation_flags()
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/.." "F-Prime")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Ref/")
# Opt-in: a second installed deployment would break GDS deployment auto-detection for the default Ref build
option(SEGMENTED_DEPLOYMENT "Build the SubtopologyBuilds/Segmented test deployment (TC Segment Header / MAP uplink)" OFF)
if (SEGMENTED_DEPLOYMENT)
    add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/SubtopologyBuilds/Segmented/")
endif()
