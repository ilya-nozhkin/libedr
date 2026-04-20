#ifndef LIBEDR_DRIVER_JTAG_JTAGCHAINACTION_HPP
#define LIBEDR_DRIVER_JTAG_JTAGCHAINACTION_HPP

#include "libedr/driver/Action.hpp"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/util/adt/BitStream.hpp"
#include "libedr/util/miscellaneous/Formatting.hpp"
#include "libedr/util/vss/VSS.hpp"
#include "libedr/util/vss/VSSPayloads.hpp"

#include <cstddef>

namespace edr {

enum class JCState : uint32_t {
  TestLogicReset = 0,
  RunTestIdle = 1,

  SelectDR = 2,
  CaptureDR = 3,
  ShiftDR = 4,
  Exit1DR = 5,
  PauseDR = 6,
  Exit2DR = 7,
  UpdateDR = 8,

  SelectIR = 9,
  CaptureIR = 10,
  ShiftIR = 11,
  Exit1IR = 12,
  PauseIR = 13,
  Exit2IR = 14,
  UpdateIR = 15,

  Unknown = 16,
};

struct JCForgetChainStructure {
  static inline constexpr auto g_id = ActionID::JCForgetChainStructure;

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("Forget-Chain-Structure");
  }

  struct Out {
    template <StructureFormatter F> void Format(F &fmt) { fmt.Value("OK"); }
  };
};

struct JCSetIRLength {
  static inline constexpr auto g_id = ActionID::JCSetIRLength;

  const uint32_t tap_id;
  const uint32_t length;

  JCSetIRLength(uint32_t tap_id, uint32_t length)
      : tap_id(tap_id), length(length) {}

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("Set-IR-Length");
    fmt.Field("TAP", "{}", tap_id);
    fmt.Field("length", "{}", length);
  }

  struct Out {
    Out(uint32_t, uint32_t) {}

    template <StructureFormatter F> void Format(F &fmt) { fmt.Value("OK"); }
  };
};

struct JCGoToState {
  static inline constexpr auto g_id = ActionID::JCGoToState;

  JCState state;
  uint32_t wait_there_cycles;

  JCGoToState(JCState state, uint32_t wait_there_cycles = 0)
      : state(state), wait_there_cycles(wait_there_cycles) {}

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("Go-To-State");
    fmt.Value("{}", state);

    if (wait_there_cycles)
      fmt.Value("+ wait {}", wait_there_cycles);
  }

  struct Out {
    Out(JCState state, uint32_t wait_there_cycles = 0) {}

    template <StructureFormatter F> void Format(F &fmt) { fmt.Value("OK"); }
  };
};

struct JCSelectTAP {
  static inline constexpr auto g_id = ActionID::JCSelectTAP;

  uint32_t tap_id;

  JCSelectTAP(uint32_t tap_id) : tap_id(tap_id) {}

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("Select-TAP");
    fmt.Value("{}", tap_id);
  }

  struct Out {
    Out(uint32_t tap_id) {}

    template <StructureFormatter F> void Format(F &fmt) { fmt.Value("OK"); }
  };
};

struct JCWriteIR {
  static inline constexpr auto g_id = ActionID::JCWriteIR;

  uint32_t value;

  JCWriteIR(uint32_t value) : value(value) {}

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("IR <-");
    fmt.Value("{}", value);
  }

  struct Out {
    Out(uint32_t value) {}

    template <StructureFormatter F> void Format(F &fmt) { fmt.Value("OK"); }
  };
};

struct JCWriteDR {
  static inline constexpr auto g_id = ActionID::JCWriteDR;

  const uint32_t num_bits;
  using Payload = vss::Payload<vss::DependentBits<uint32_t>>;

  template <class T>
  JCWriteDR(const BitStream<T> &bits) : num_bits(bits.GetNumBits()) {}

  JCWriteDR(uint64_t bits, unsigned num_bits) : num_bits(num_bits) {}

  BitStream<const uint32_t> Bits() {
    return vss::Get<0>(*this).Stream(num_bits);
  }

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("DR <-");
    fmt.Value("{:r}", Bits());
  }

  template <class T>
  static void EmplacePayload(vss::OutputStream auto &os,
                             const BitStream<T> &bits) {
    Payload::Emplace(os, bits);
  }

  static void EmplacePayload(vss::OutputStream auto &os, uint64_t bits,
                             unsigned num_bits) {
    BitStream<uint64_t> bit_stream(&bits, num_bits);
    Payload::Emplace(os, bit_stream);
  }

  std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size) {
    return payload.Size(max_size, num_bits);
  }

  struct Out {
    template <class T> Out(const BitStream<T> &bits) {}

    Out(uint64_t bits, unsigned num_bits) {}

    template <StructureFormatter F> void Format(F &fmt) { fmt.Value("OK"); }
  };

  JCWriteDR(const JCWriteDR &) = delete;
  JCWriteDR(JCWriteDR &&) = delete;
};

struct JCShiftDR {
  static inline constexpr auto g_id = ActionID::JCShiftDR;

  const uint32_t num_bits;
  using Payload = vss::Payload<vss::DependentBits<uint32_t>>;

  template <class T>
  JCShiftDR(const BitStream<T> &bits) : num_bits(bits.GetNumBits()) {}

  JCShiftDR(uint64_t bits, unsigned num_bits) : num_bits(num_bits) {}

  BitStream<const uint32_t> Bits() {
    return vss::Get<0>(*this).Stream(num_bits);
  }

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("DR <->");
    fmt.Value("{:r}", Bits());
  }

  template <class T>
  static void EmplacePayload(vss::OutputStream auto &os,
                             const BitStream<T> &bits) {
    Payload::Emplace(os, bits);
  }

  static void EmplacePayload(vss::OutputStream auto &os, uint64_t bits,
                             unsigned num_bits) {
    BitStream<uint64_t> bit_stream(&bits, num_bits);
    Payload::Emplace(os, bit_stream);
  }

  std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size) {
    return payload.Size(max_size, num_bits);
  }

  struct Out {
    uint32_t num_bits;
    using Payload = vss::Payload<vss::DependentBits<uint32_t>>;

    template <class T> Out(const BitStream<T> &bits) {}

    Out(uint64_t bits, unsigned num_bits) {}

    BitStream<uint32_t> Bits(size_t num_bits) {
      return vss::Get<0>(*this).Stream(num_bits);
    }

    BitStream<uint32_t> Bits() { return vss::Get<0>(*this).Stream(num_bits); }

    template <StructureFormatter F> void Format(F &fmt) {
      fmt.Value("{:r}", Bits());
    }

    template <class T>
    static void EmplacePayload(vss::OutputStream auto &os,
                               const BitStream<T> &bits) {
      Payload::Emplace(os, bits.GetNumBits());
    }

    static void EmplacePayload(vss::OutputStream auto &os, uint64_t bits,
                               unsigned num_bits) {
      Payload::Emplace(os, num_bits);
    }

    std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size,
                                        JCShiftDR &action) {
      return payload.Size(max_size, action.num_bits);
    }

    Out(const Out &) = delete;
    Out(Out &&) = delete;
  };

  JCShiftDR(const JCShiftDR &) = delete;
  JCShiftDR(JCShiftDR &&) = delete;
};

struct CauseInvalidJtagTapID {
  static inline constexpr auto g_id = CauseID::InvalidJtagTapID;

  const uint32_t requested_id;
  const uint32_t max_id;

  CauseInvalidJtagTapID(uint32_t requested_id, uint32_t max_id)
      : requested_id(requested_id), max_id(max_id) {}

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Value("Invalid TAP ID {}, max valid ID is {}", requested_id, max_id);
  }
};

struct CauseInvalidJtagState {
  static inline constexpr auto g_id = CauseID::InvalidJtagState;

  const unsigned state_id;

  CauseInvalidJtagState(unsigned state_id) : state_id(state_id) {}

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Value("Invalid JTAG state {}", state_id);
  }
};

struct CauseUnstableJtagState {
  static inline constexpr auto g_id = CauseID::UnstableJtagState;

  const JCState state;

  CauseUnstableJtagState(JCState state) : state(state) {}

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Value("Cannot wait in {} state - it is unstable", state);
  }
};

struct CauseIRLengthTooBig {
  static inline constexpr auto g_id = CauseID::IRLengthTooBig;

  const uint32_t requested;
  const uint32_t max_length;

  CauseIRLengthTooBig(uint32_t requested, uint32_t max_length)
      : requested(requested), max_length(max_length) {}

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Value("The specified IR length {} is greater than the maximum "
              "supported length {}",
              requested, max_length);
  }
};

using JtagChainAction =
    Action<JCForgetChainStructure, JCSetIRLength, JCGoToState, JCSelectTAP,
           JCWriteIR, JCWriteDR, JCShiftDR>;

} // namespace edr

template <> struct std::formatter<edr::JCState, char> {
  template <class ParseContext>
  constexpr ParseContext::iterator parse(ParseContext &ctx) {
    auto it = ctx.begin();
    while (*it != '}')
      it++;

    return it;
  }

  template <class FmtContext>
  FmtContext::iterator format(const edr::JCState &state,
                              FmtContext &ctx) const {
    switch (state) {
    case edr::JCState::TestLogicReset:
      return std::format_to(ctx.out(), "TestLogicReset");
    case edr::JCState::RunTestIdle:
      return std::format_to(ctx.out(), "RunTestIdle");
    case edr::JCState::SelectDR:
      return std::format_to(ctx.out(), "SelectDR");
    case edr::JCState::CaptureDR:
      return std::format_to(ctx.out(), "CaptureDR");
    case edr::JCState::ShiftDR:
      return std::format_to(ctx.out(), "ShiftDR");
    case edr::JCState::Exit1DR:
      return std::format_to(ctx.out(), "Exit1DR");
    case edr::JCState::PauseDR:
      return std::format_to(ctx.out(), "PauseDR");
    case edr::JCState::Exit2DR:
      return std::format_to(ctx.out(), "Exit2DR");
    case edr::JCState::UpdateDR:
      return std::format_to(ctx.out(), "UpdateDR");
    case edr::JCState::SelectIR:
      return std::format_to(ctx.out(), "SelectIR");
    case edr::JCState::CaptureIR:
      return std::format_to(ctx.out(), "CaptureIR");
    case edr::JCState::ShiftIR:
      return std::format_to(ctx.out(), "ShiftIR");
    case edr::JCState::Exit1IR:
      return std::format_to(ctx.out(), "Exit1IR");
    case edr::JCState::PauseIR:
      return std::format_to(ctx.out(), "PauseIR");
    case edr::JCState::Exit2IR:
      return std::format_to(ctx.out(), "Exit2IR");
    case edr::JCState::UpdateIR:
      return std::format_to(ctx.out(), "UpdateIR");
    default:
      return std::format_to(ctx.out(), "<unknown>");
    }
  }
};

#endif
