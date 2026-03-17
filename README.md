A library of drivers for various hardware interfaces
====================================================

``libedr`` defines abstract asynchronous C++, C and Python APIs for some hardware interfaces and provides implementations for some common cases such SystemVerilog modules implemented using DPI.

It also supports serialization of all defined actions out of the box which enables tunneling the transactions between processes via named pipes or between hosts via TCP.

One of notable use cases is connecting to an RTL simulator running in a separate process to drive the simulated interfaces either from Python-based scenarios or from an interactive debugger implemented in C/C++.
And since all driver APIs are abstract, it should also be possible to implement them using a physical debug probe's API to reuse the same test scenarios and debug adapters when connecting to an FPGA.

The architecture is designed with high-latency connections in mind.
Transactions consist of sequences of actions that can be processed in batches.

Supported interfaces and tunnels
--------------------------------

Currently, the only supported interface is JTAG.

Tunnelling is possible via any bi-directional byte stream, TCP sockets and Unix domain sockets are supported out of the box.

How to build
------------

If tests and Python and C APIs are not needed, then this should be enough:

```
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=../install ..
cmake --build . --target install
```

To enable all optional features including all APIs and tests, execute this:

```
mkdir build
cd build
cmake \
  -DCMAKE_INSTALL_PREFIX=../install \
  -DEDR_PYTHON3_EXECUTABLE=<path to a python3 executable that knows its headers (e.g. installed using 'apt install python3-dev')> \
  -DEDR_GTEST_CMAKE_PACKAGE_PATH=<path to GoogleTest installation>/lib/cmake/GTest \
  -DEDR_SWIG_EXECUTABLE=<path to a SWIG installation that supports C as a target>/bin/swig \
  -DEDR_VERILATOR_EXECUTABLE=<path to a Verilator installation>/bin/verilator
  ..
cmake --build . --target install
```

There are ``install_*.sh`` scripts in ``build_linux`` directory that can help with compiling the dependencies from the source code.

Example: Reading IDCODE from a JTAG TAP simulated by Verilator using Python
---------------------------------------------------------------------------

Here: [examples/jtag_idcode](examples/jtag_idcode)

How to run:

1. Build and install the library following the steps from **How to build** section, make sure that both SWIG and Python are found by the build system (the CMake scripts print warnings if this is not the case).
2. ``cd`` to ``examples/jtag_idcode``
3. If you set ``CMAKE_INSTALL_PREFIX`` to anything other than ``../install`` when your were building the library, then open ``build.sh`` and ``test.py`` scripts and adjust the relative paths there accordingly.
4. Make sure ``verilator`` is visible from ``PATH`` by running ``verilator --version``
5. Execute ``./build.sh``
6. Execute ``python3 ./test.py``

The expected output is:
```
IDCODE is 0x149511c3, should be 0x149511c3
```

Check comments in [examples/jtag_idcode/test.py](examples/jtag_idcode/test.py) for a detailed explanation of how the example works.
Check [examples/jtag_idcode/testbench.sv](examples/jtag_idcode/testbench.sv) and [examples/jtag_idcode/tunneled_jtag.sv](examples/jtag_idcode/tunneled_jtag.sv) to see how a driver and a tunnel can be instantiated in SystemVerilog.
