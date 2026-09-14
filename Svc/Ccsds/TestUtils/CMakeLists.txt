if (BUILD_TESTING)
  register_fprime_library(
    SOURCES
      "${CMAKE_CURRENT_LIST_DIR}/TestUtils.cpp"
    DEPENDS
      Fw_Buffer
      Svc_Ccsds_Types
      Svc_Ccsds_Utils
      STest
  )
endif()

register_fprime_ut(
  SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
)
