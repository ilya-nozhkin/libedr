SCRIPT_DIR=$(dirname $0)

EDR_INSTALL=${SCRIPT_DIR}/../../install
EDR_INSTALL=$(realpath ${EDR_INSTALL})

FREECORES_JTAG=${SCRIPT_DIR}/../../third_party/freecores/jtag

verilator \
  --timing --binary \
  -Wall -CFLAGS -std=c++20 \
  -Wno-WIDTHTRUNC \
  -Wno-UNUSEDSIGNAL \
  -j $(nproc) \
  --top testbench \
  -I${EDR_INSTALL}/verilog \
  -I${FREECORES_JTAG}/tap/rtl/verilog \
  ${SCRIPT_DIR}/testbench.sv \
  ${SCRIPT_DIR}/tunneled_jtag.sv \
  ${FREECORES_JTAG}/tap/rtl/verilog/tap_top.v \
  ${EDR_INSTALL}/lib/libcedr.so
