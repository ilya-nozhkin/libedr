include(GoogleTest)

function(edr_tests)
  set(PATH ${CMAKE_CURRENT_LIST_DIR})
  cmake_path(GET PATH STEM DIRECTORY)
  set(TARGET test_${DIRECTORY})

  add_executable(${TARGET} ${ARGN})

  target_include_directories(${TARGET} PRIVATE ${TEST_ROOT_DIR}/../)
  
  target_link_libraries(${TARGET}
    GTest::gtest_main
    GTest::gmock_main
    libedr
  )
  
  gtest_discover_tests(${TARGET})
endfunction()

add_subdirectory(tunnel)
add_subdirectory(util)
add_subdirectory(driver)
