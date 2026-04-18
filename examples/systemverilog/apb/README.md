This example shows how to control an APB target from an "initial" block using an OOP-style API without any external processes.

How to run:

1. Set ``EDR_INSTALL_DIR`` environment variable so that it points to a libedr distribution (follow the steps from **How to build** section to build one, make sure that both SWIG and Python are found by the build system).
2. Make sure ``verilator`` is visible from ``PATH`` by running ``verilator --version``.
3. Execute ``./build.sh``
4. Execute ``./run.sh``

The expected output is:
```
Wrote 0 <- 11223344
Wrote 8 <- aabbccdd
Read 0 -> 11223344
Read 8 -> aabbccdd
Wrote 8 <- 11223344
Read 0 -> 11223344
Read 8 -> 11223344
```

Check [testbench.sv](testbench.sv) for a detailed explanation on how to instantiate the driver and how to use its API.

To reproduce this using a different simulator, add these files to the build: \
DPI imports and classes: ``${EDR_INSTALL_DIR}/verilog/edr_sv_api.sv`` \
DPI implementations: ``${EDR_INSTALL_DIR}/lib/libcedr.so``
