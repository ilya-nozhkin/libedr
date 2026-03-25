SCRIPT_DIR=$(dirname $0)

EDR_INSTALL=${SCRIPT_DIR}/../../install
EDR_INSTALL=$(realpath ${EDR_INSTALL})

APB=${SCRIPT_DIR}/../../third_party/apb

verilator \
  --timing --binary \
  -Wall -CFLAGS -std=c++20 \
  -Wno-WIDTHTRUNC \
  -Wno-WIDTHEXPAND \
  -Wno-UNUSEDPARAM \
  -Wno-UNUSEDSIGNAL \
  -j $(nproc) \
  --top testbench \
  -I${EDR_INSTALL}/verilog \
  -I${APB} \
  ${SCRIPT_DIR}/testbench.sv \
  ${SCRIPT_DIR}/tunneled_apb.sv \
  ${APB}/apb_slave.sv \
  ${EDR_INSTALL}/lib/libcedr.so
