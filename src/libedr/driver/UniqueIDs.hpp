#ifndef LIBEDR_DRIVER_UNIQUEIDS_HPP
#define LIBEDR_DRIVER_UNIQUEIDS_HPP

#include <cstdint>

namespace edr {

enum class DriverID : uint32_t {
  ByteStream = 0,
  Jtag = 1000,
  ExecutionGate = 2000,
  APB = 3000,
};

constexpr uint32_t ActionOffset(DriverID driver_id) {
  return static_cast<uint32_t>(driver_id);
}

enum class ActionID : uint32_t {
  // Common
  InvalidAction = 0,
  SkipCycles = 1,

  // ByteStream
  WriteBytes = 1 + ActionOffset(DriverID::ByteStream),
  ReadBytes = 2 + ActionOffset(DriverID::ByteStream),

  // JTAG
  PutTMS = 1 + ActionOffset(DriverID::Jtag),
  PutTDI = 2 + ActionOffset(DriverID::Jtag),
  PutTDIGetTDO = 3 + ActionOffset(DriverID::Jtag),

  // ExecutionGate
  SetExecutionGateMode = 1 + ActionOffset(DriverID::ExecutionGate),

  // APB
  APBWrite = 1 + ActionOffset(DriverID::APB),
  APBRead = 2 + ActionOffset(DriverID::APB),
  APBSetPSEL = 3 + ActionOffset(DriverID::APB),
};

enum class CauseID : uint32_t {
  UnsupportedAction = 0,
  AllocationFailure = 1,
  StringMessage = 2,
  NestedError = 3,
  TooMuchData = 4,
  CauseTerminated = 5,
  CauseErrno = 6,
  CauseInvalidArgument = 7,
  CauseTimeoutInCycles = 8,
  CauseTargetError = 9,
};

} // namespace edr

#endif
