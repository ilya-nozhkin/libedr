#ifndef LIBEDR_DRIVER_UNIQUEIDS_HPP
#define LIBEDR_DRIVER_UNIQUEIDS_HPP

#include <cstdint>

namespace edr {

enum class DriverID : uint32_t {
  ByteStream = 0,
  Jtag = 1000,
  ExecutionGate = 2000,
  APB = 3000,
  JtagChain = 4000,
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

  // JtagChain
  JCForgetChainStructure = 1 + ActionOffset(DriverID::JtagChain),
  JCSetIRLength = 2 + ActionOffset(DriverID::JtagChain),
  JCGoToState = 3 + ActionOffset(DriverID::JtagChain),
  JCSelectTAP = 4 + ActionOffset(DriverID::JtagChain),
  JCWriteIR = 5 + ActionOffset(DriverID::JtagChain),
  JCWriteDR = 6 + ActionOffset(DriverID::JtagChain),
  JCShiftDR = 7 + ActionOffset(DriverID::JtagChain),
};

enum class CauseID : uint32_t {
  UnsupportedAction = 0,
  AllocationFailure = 1,
  StringMessage = 2,
  NestedError = 3,
  TooMuchData = 4,
  Terminated = 5,
  Errno = 6,
  InvalidArgument = 7,
  TimeoutInCycles = 8,
  TargetError = 9,
  InvalidJtagTapID = 10,
  InvalidJtagState = 11,
  UnstableJtagState = 12,
  IRLengthTooBig = 13,
};

} // namespace edr

#endif
