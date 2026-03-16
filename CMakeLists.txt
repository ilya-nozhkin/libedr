cmake_minimum_required(VERSION 3.10.0)
project(libedr VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# [CONFIGURATION]

set(EDR_GTEST_CMAKE_PACKAGE_PATH "" CACHE STRING
    "Path to googletest/lib/cmake/GTest")

set(EDR_SWIG_EXECUTABLE "swig" CACHE STRING
    "Path to a swig binary, minimum version is 4.4")

set(EDR_PYTHON3_EXECUTABLE "python3" CACHE STRING
    "Path to a Python 3 executable")

set(EDR_VERILATOR_EXECUTABLE "verilator" CACHE STRING
    "Path to verilator")

# [DEPENDENCIES]

find_package(GTest HINTS ${EDR_GTEST_CMAKE_PACKAGE_PATH})

execute_process(
  COMMAND ${EDR_PYTHON3_EXECUTABLE} -c "import sysconfig; print(sysconfig.get_paths()['include'])"
  OUTPUT_VARIABLE EDR_PYTHON3_INCLUDE_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
  COMMAND ${EDR_VERILATOR_EXECUTABLE} --version
  OUTPUT_VARIABLE EDR_VERILATOR_VERSION_OUTPUT
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
  COMMAND ${EDR_SWIG_EXECUTABLE} --version
  OUTPUT_VARIABLE EDR_SWIG_VERSION_OUTPUT
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# [PROJECT]

set(EDR_VERILOG_DIR ${CMAKE_CURRENT_LIST_DIR}/src/verilog)
set(EDR_THIRD_PARTY_DIR ${CMAKE_CURRENT_LIST_DIR}/third_party)

add_subdirectory(src)

include(CTest)
enable_testing()

add_subdirectory(test)
