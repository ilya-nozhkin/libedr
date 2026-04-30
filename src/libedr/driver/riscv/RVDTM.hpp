#ifndef LIBEDR_DRIVER_RISCV_RVDTM_HPP
#define LIBEDR_DRIVER_RISCV_RVDTM_HPP

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/driver/riscv/RVDTMAction.hpp"

namespace edr {

using RVDTM = Driver<DriverID::RVDTM, RVDTMAction>;

} // namespace edr

#endif
