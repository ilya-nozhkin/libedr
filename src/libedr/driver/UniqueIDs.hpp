#ifndef LIBEDR_DRIVER_UNIQUEIDS_HPP
#define LIBEDR_DRIVER_UNIQUEIDS_HPP

#include <cstdint>

namespace edr {

enum class DriverID : uint32_t {
  ByteStream = 0,
};

enum class ActionID : uint32_t {
  InvalidAction = 0,

  // ByteStream
  WriteBytes = 1,
  ReadBytes = 2,
};

enum class CauseID : uint32_t {
  UnsupportedAction = 0,
  AllocationFailure = 1,
  StringMessage = 2,
  NestedError = 3,
  TooMuchData = 4,
  CauseTerminated = 5,
  CauseErrno = 6,
};

} // namespace edr

#endif
