add_custom_target(python_test_dependencies)

function(add_verilated_model TARGET_NAME)
  cmake_parse_arguments(
    ARG
    ""
    ""
    "SOURCES;ARGS"
    ${ARGN}
  )

  set(EDR_C_API_LIBRARY ${EDR_API_BUILD_DIR}/libcedr.so)

  add_custom_target(${TARGET_NAME}
    COMMAND ${EDR_VERILATOR_EXECUTABLE} --timing --trace --binary -Wall -CFLAGS -std=c++20 --top ${TARGET_NAME} ${ARG_ARGS} ${ARG_SOURCES} ${EDR_C_API_LIBRARY}
    DEPENDS ${ARG_SOURCES}
  )

  add_dependencies(python_test_dependencies ${TARGET_NAME})
endfunction()

set(PYTHON_TESTS_DIR ${CMAKE_CURRENT_LIST_DIR})

function(add_python_tests_from_current_directory)
  set(PATH ${CMAKE_CURRENT_LIST_DIR})
  cmake_path(GET PATH STEM DIRECTORY)

  cmake_parse_arguments(
    ARG
    ""
    ""
    "DEPENDENCIES"
    ${ARGN}
  )

  if (ARG_DEPENDENCIES)
    add_test(build_${DIRECTORY}_dependencies
      "${CMAKE_COMMAND}"
      --build "${CMAKE_BINARY_DIR}"
      --config "$<CONFIG>"
      --target ${ARG_DEPENDENCIES}
    )
    
    set_tests_properties(build_${DIRECTORY}_dependencies PROPERTIES FIXTURES_SETUP ${DIRECTORY}_fixture)
  endif()

  execute_process(
    COMMAND ${EDR_PYTHON3_EXECUTABLE} ${PYTHON_TESTS_DIR}/launcher.py --syspath ${PYTHON_TESTS_DIR}/../python_stubs --list ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE EDR_PYTHON_TESTS_OUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
  )

  separate_arguments(EDR_PYTHON_TESTS UNIX_COMMAND ${EDR_PYTHON_TESTS_OUT})

  foreach(TEST IN LISTS EDR_PYTHON_TESTS)
    add_test(
      NAME ${TEST}
      COMMAND ${EDR_PYTHON3_EXECUTABLE} ${PYTHON_TESTS_DIR}/launcher.py --syspath ${EDR_API_BUILD_DIR} ${CMAKE_CURRENT_LIST_DIR} ${TEST}
    )
  
    if (ARG_DEPENDENCIES)
      set_tests_properties(${TEST} PROPERTIES FIXTURES_REQUIRED ${DIRECTORY}_fixture)
    endif()
  endforeach()
endfunction()

add_subdirectory(jtag_verilator)
add_subdirectory(tcp_socket_loopback)

