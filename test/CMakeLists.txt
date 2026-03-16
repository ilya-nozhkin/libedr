set(TEST_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

if (NOT IS_DIRECTORY ${EDR_PYTHON3_INCLUDE_DIR})
  message(WARNING
    "Python 3 include directory was not found, "
    "Python API tests will not be configured, "
    "provide a path to a python3 executable with development "
    "support using EDR_PYTHON3_EXECUTABLE parameter")
else()
  if (NOT EDR_VERILATOR_VERSION_OUTPUT)
    message(WARNING
      "Verilator executable not found, Python API tests will not be configured "
      "provide a path to verilator binary using "
      "EDR_VERILATOR_EXECUTABLE parameter")
  else()
    if (NOT EDR_API_BUILD_DIR)
      message(WARNING
        "The API is not configured to be built, skipping API tests")
    else()
      add_subdirectory(python)
    endif()
  endif()
endif()

if (NOT GTest_FOUND)
  message(WARNING
    "GTest not found, unit tests will not be configured, "
    "provide a path to googletest/lib/cmake/GTest directory "
    "using EDR_GTEST_CMAKE_PACKAGE_PATH parameter")
else()
  add_subdirectory(unit)
endif()
