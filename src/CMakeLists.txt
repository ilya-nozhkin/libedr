add_subdirectory(libedr)
add_subdirectory(verilog)

if (NOT EDR_SWIG_VERSION_OUTPUT)
  message(WARNING
    "SWIG not found, C and Python API will not be configured, "
    "provide a path to a SWIG executable using "
    "EDR_SWIG_EXECUTABLE parameter")
else()
  add_subdirectory(api)
endif()
