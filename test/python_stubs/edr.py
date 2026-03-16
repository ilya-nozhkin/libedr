# This will make this module look like it contains any requested attribute
# This allows CMake to discover tests by importing them even if they
# reference the edr module which will be built later.
globals()["__getattr__"] = lambda item: None
