%module("threads"=1) edr

%feature("autodoc", "1");

%nodefaultctor;
%nothread;

%{
@EDR_API_C_INCLUDES@
%}

%include <pybuffer.i>
%include <stdint.i>
%include <std_shared_ptr.i>

%define %mutable_bits(BITS_PTR)
%typemap(in) (BITS_PTR)(size_t size) {
  size = 0;
  Py_buffer view;
  int res = PyObject_GetBuffer($input, &view, PyBUF_WRITABLE);
  if (res < 0)
    %argument_fail(res, "(BITS_PTR)", $symname, $argnum);

  size = view.len;
  auto buf = view.buf;
  PyBuffer_Release(&view);
  $1 = ($1_ltype) buf;
}
%enddef

%define %const_bits(BITS_PTR)
%typemap(in) (BITS_PTR)(size_t size) {
  size = 0;
  Py_buffer view;
  int res = PyObject_GetBuffer($input, &view, PyBUF_CONTIG_RO);
  if (res < 0)
    %argument_fail(res, "(BITS_PTR)", $symname, $argnum);

  size = view.len;
  auto buf = view.buf;
  PyBuffer_Release(&view);
  $1 = ($1_ltype) buf;
}
%enddef

%define %check_bit_storage_size(NUM_BITS, STORAGE_SIZE_NAME)
%typemap(check) (NUM_BITS) {
  if ($1 > 8 * STORAGE_SIZE_NAME)
    SWIG_exception_fail(SWIG_ValueError, "The requested number of bits does not fit into the provided storage.");
}
%enddef

%shared_ptr(Context)

%pybuffer_binary(const std::byte *src, size_t size)
%pybuffer_mutable_binary(std::byte *dest, size_t size)

%mutable_bits(std::byte *dest_bits);
%check_bit_storage_size(size_t max_num_bits, size2)

%mutable_bits(std::byte *tms_dest);
%check_bit_storage_size(size_t max_tms_bits, size2)

%mutable_bits(std::byte *tdi_dest);
%check_bit_storage_size(size_t max_tdi_bits, size4)

%const_bits(const std::byte *src_bits);
%check_bit_storage_size(size_t num_bits, size2)

@EDR_API_SWIG_INCLUDES@
