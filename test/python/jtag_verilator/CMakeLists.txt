add_verilated_model(jtag_verilator
  SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/jtag_verilator.sv
    ${EDR_THIRD_PARTY_DIR}/freecores/jtag/tap/rtl/verilog/tap_top.v

  ARGS
    -I${EDR_THIRD_PARTY_DIR}/freecores/jtag/tap/rtl/verilog
    -Wno-WIDTHTRUNC
    -Wno-UNUSEDSIGNAL
)

add_python_tests_from_current_directory(
  DEPENDENCIES jtag_verilator
)
