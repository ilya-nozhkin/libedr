%module edr

%nodefaultctor;

%{
@EDR_API_C_INCLUDES@
%}

%include <stdint.i>
%include <std_shared_ptr.i>

%shared_ptr(Context)

namespace std {
  typedef uint8_t byte;
}

@EDR_API_SWIG_INCLUDES@
