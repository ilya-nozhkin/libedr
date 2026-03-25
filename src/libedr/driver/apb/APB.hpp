#ifndef LIBEDR_DRIVER_APB_APB_HPP
#define LIBEDR_DRIVER_APB_APB_HPP

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/driver/apb/APBAction.hpp"

namespace edr {

using APB = Driver<DriverID::APB, APBAction>;

} // namespace edr

#endif
