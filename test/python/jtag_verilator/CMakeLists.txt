add_verilated_model(jtag_verilator
  SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/../common_verilog/test_tunnel.sv
    ${CMAKE_CURRENT_LIST_DIR}/jtag_verilator.sv
    ${EDR_VERILOG_DIR}/driver/jtag/edr_jtag.sv
    ${EDR_THIRD_PARTY_DIR}/freecores/jtag/tap/rtl/verilog/tap_top.v

  ARGS
    -I${EDR_VERILOG_DIR}
    -I${CMAKE_CURRENT_LIST_DIR}/../common_verilog
    -I${EDR_THIRD_PARTY_DIR}/freecores/jtag/tap/rtl/verilog
    -Wno-WIDTHTRUNC
    -Wno-UNUSEDSIGNAL
)

add_python_tests_from_current_directory(
  DEPENDENCIES jtag_verilator
)
