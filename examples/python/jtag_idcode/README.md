This example shows how to control a JTAG TAP simulated by Verilator from a Python script running in a separate process.

How to run:

1. Set ``EDR_INSTALL_DIR`` environment variable so that it points to a libedr distribution (follow the steps from **How to build** section to build one, make sure that both SWIG and Python are found by the build system).
2. Make sure ``verilator`` is visible from ``PATH`` by running ``verilator --version``.
3. Execute ``./build.sh``
4. Execute ``python3 ./test.py``

The expected output is:
```
IDCODE is 0x149511c3, should be 0x149511c3
```

Check comments in [test.py](test.py) for a detailed explanation of how the example works.
Check [testbench.sv](testbench.sv) to see how the driver and the inter-process tunnel can be instantiated in SystemVerilog.

To reproduce this using a different simulator, add these files to the build: \
DPI imports and classes: ``${EDR_INSTALL_DIR}/verilog/edr_sv_api.sv`` \
DPI implementations: ``${EDR_INSTALL_DIR}/lib/libcedr.so``
