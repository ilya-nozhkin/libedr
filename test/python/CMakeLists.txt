#function(edr_python_test NAME TEST_MODULE_NAME)
#  add_test(
#    NAME ${NAME}
#    COMMAND ${EDR_PYTHON3_EXECUTABLE} -c "import sys; sys.path.append('${EDR_API_BUILD_DIR}'); import ${TEST_MODULE_NAME}")
#endfunction()

execute_process(
  COMMAND ${EDR_PYTHON3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/launcher.py --syspath ${EDR_API_BUILD_DIR} --list
  OUTPUT_VARIABLE EDR_PYTHON_TESTS_OUT
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

separate_arguments(EDR_PYTHON_TESTS UNIX_COMMAND ${EDR_PYTHON_TESTS_OUT})

foreach(TEST IN LISTS EDR_PYTHON_TESTS)
  add_test(
    NAME ${TEST}
    COMMAND ${EDR_PYTHON3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/launcher.py --syspath ${EDR_API_BUILD_DIR} ${TEST})
endforeach()
