#!/bin/bash

SCRIPT_DIR=$(dirname $0)

if [[ ! -v EDR_INSTALL_DIR ]]; then
  echo "EDR_INSTALL_DIR environment variable is not set. Point it to an EDR installation directory."
  exit
fi

FREECORES_JTAG=${SCRIPT_DIR}/../../third_party/freecores/jtag

verilator \
  --timing --binary \
  --top testbench \
  ${EDR_INSTALL_DIR}/lib/libcedr.so \
  ${EDR_INSTALL_DIR}/verilog/edr_sv_api.sv \
  -I${FREECORES_JTAG}/tap/rtl/verilog \
  ${FREECORES_JTAG}/tap/rtl/verilog/tap_top.v \
  ${SCRIPT_DIR}/testbench.sv \
  -Wall -CFLAGS -std=c++20 \
  -Wno-WIDTHTRUNC \
  -Wno-UNUSEDSIGNAL \
  -j $(nproc)
