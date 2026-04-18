#!/bin/bash

SCRIPT_DIR=$(dirname $0)

if [[ ! -v EDR_INSTALL_DIR ]]; then
  echo "EDR_INSTALL_DIR environment variable is not set. Point it to a libedr installation directory."
  exit
fi

APB=${SCRIPT_DIR}/../../../third_party/apb

verilator \
  --timing --binary \
  --top testbench \
  -I${APB} \
  ${EDR_INSTALL_DIR}/lib/libcedr.so \
  ${EDR_INSTALL_DIR}/verilog/edr_sv_api.sv \
  ${APB}/apb_slave.sv \
  ${SCRIPT_DIR}/testbench.sv \
  -j $(nproc) \
  -Wall -CFLAGS -std=c++20 \
  -Wno-WIDTHTRUNC \
  -Wno-WIDTHEXPAND \
  -Wno-UNUSEDPARAM \
  -Wno-UNUSEDSIGNAL
