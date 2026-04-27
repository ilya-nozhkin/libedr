add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/veer_eh2_config/eh2_pdef.vh
  COMMAND 
    ${CMAKE_COMMAND} -E env
      BUILD_PATH=${CMAKE_CURRENT_BINARY_DIR}/veer_eh2_config
      RV_ROOT=${EDR_THIRD_PARTY_DIR}/Cores-VeeR-EH2
      ${EDR_THIRD_PARTY_DIR}/Cores-VeeR-EH2/configs/veer.config
  DEPENDS
    ${EDR_THIRD_PARTY_DIR}/Cores-VeeR-EH2/configs/veer.config
)

add_verilated_model(veer_eh2_debug
  TOP_MODULE
    tb_top

  SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/tb_top.sv
    ${EDR_THIRD_PARTY_DIR}/Cores-VeeR-EH2/testbench/ahb_sif.sv

  ARGS
    -I${CMAKE_CURRENT_BINARY_DIR}/veer_eh2_config
    ${CMAKE_CURRENT_LIST_DIR}/veer_eh2_verilator_config.vh
    ${CMAKE_CURRENT_BINARY_DIR}/veer_eh2_config/eh2_pdef.vh
    -I${EDR_THIRD_PARTY_DIR}/Cores-VeeR-EH2/testbench
    -f ${EDR_THIRD_PARTY_DIR}/Cores-VeeR-EH2/design/flist
    -Wno-WIDTH
    -Wno-UNOPTFLAT
    -Wno-IMPLICIT
    -Wno-PINCONNECTEMPTY
    -Wno-GENUNNAMED
    -Wno-DECLFILENAME
    -Wno-EOFNEWLINE
    -Wno-VARHIDDEN
    -Wno-BLKSEQ
    -Wno-UNUSEDSIGNAL
    -Wno-UNDRIVEN
    -Wno-UNUSEDPARAM
    -Wno-SYNCASYNCNET

  ENVIRONMENT
    RV_ROOT=${EDR_THIRD_PARTY_DIR}/Cores-VeeR-EH2

  DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/veer_eh2_config/eh2_pdef.vh
)

add_python_tests_from_current_directory(
  DEPENDENCIES veer_eh2_debug
)
