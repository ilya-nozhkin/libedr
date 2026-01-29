%module edr

%feature("autodoc", "1");

%nodefaultctor;

%{
@EDR_API_C_INCLUDES@
%}

%include <pybuffer.i>
%include <stdint.i>
%include <std_shared_ptr.i>

%shared_ptr(Context)

%pybuffer_binary(const std::byte *src, size_t size)
%pybuffer_mutable_binary(std::byte *dest, size_t size)

@EDR_API_SWIG_INCLUDES@
