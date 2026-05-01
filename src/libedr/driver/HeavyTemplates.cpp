#include <libedr/driver/DriverImpl.hpp>

namespace edr {

#define DEFINE_EXTERN_DRIVER_INSTANTIATION(DRIVER)                             \
  template class TransactionBuilder<DRIVER##Action>;                           \
  template class TransactionInProgress<DRIVER##Action>;                        \
  template class TransactionStatus<DRIVER##Action>;                            \
  template class Driver<DriverID::DRIVER, DRIVER##Action>;

ALL_EDR_DRIVERS(DEFINE_EXTERN_DRIVER_INSTANTIATION)

#undef DEFINE_EXTERN_DRIVER_INSTANTIATION

} // namespace edr
