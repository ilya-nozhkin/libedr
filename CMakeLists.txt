cmake_minimum_required(VERSION 3.10.0)
project(libedr VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# [CONFIGURATION]

set(EDR_GTEST_CMAKE_PACKAGE_PATH "" CACHE STRING
    "Path to googletest/lib/cmake/GTest")

set(EDR_SWIG_EXECUTABLE "" CACHE STRING
    "Path to a swig binary, minimum version is 4.4")

set(EDR_PYTHON3_INCLUDE_DIR "" CACHE STRING
    "Path to a directory that contains Python.h")

set(EDR_PYTHON3_EXECUTABLE "" CACHE STRING
    "Path to a Python 3 executable")

# [DEPENDENCIES]

find_package(GTest PATHS ${EDR_GTEST_CMAKE_PACKAGE_PATH})

# [PROJECT]

add_subdirectory(src)

include(CTest)
enable_testing()

add_subdirectory(test)
