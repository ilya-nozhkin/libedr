add_verilated_model(apb_verilator
  SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/apb_verilator.sv
    ${EDR_THIRD_PARTY_DIR}/apb/apb_slave.sv

  ARGS
    -I${EDR_THIRD_PARTY_DIR}/apb
    -Wno-WIDTHTRUNC
    -Wno-UNUSEDSIGNAL
    -Wno-WIDTHEXPAND
    -Wno-UNUSEDPARAM
)

add_python_tests_from_current_directory(
  DEPENDENCIES apb_verilator
)
