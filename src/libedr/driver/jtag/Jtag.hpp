#ifndef LIBEDR_DRIVER_JTAG_JTAG_HPP
#define LIBEDR_DRIVER_JTAG_JTAG_HPP

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/driver/jtag/JtagAction.hpp"

namespace edr {

using Jtag = Driver<DriverID::Jtag, JtagAction>;

} // namespace edr

#endif
