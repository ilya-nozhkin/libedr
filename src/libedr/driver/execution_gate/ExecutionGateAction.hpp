
#ifndef LIBEDR_DRIVER_EXECUTION_GATE_EXECUTIONGATEACTION_HPP
#define LIBEDR_DRIVER_EXECUTION_GATE_EXECUTIONGATEACTION_HPP

#include "libedr/driver/Action.hpp"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/util/miscellaneous/Formatting.hpp"

namespace edr {

enum class ExecutionGateMode : uint32_t {
  StallIfNoRequestsAndIdle = 0, // Default
  ForceStallIfNoRequsts = 1,
  ForceStall = 2,
  NoStall = 3,
  Terminated = 4,
};

struct SetExecutionGateMode {
  static inline constexpr auto g_id = ActionID::SetExecutionGateMode;

  ExecutionGateMode mode;

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("SET-MODE");
    fmt.Value("{}", mode);
  }

  SetExecutionGateMode(ExecutionGateMode mode) : mode(mode) {}

  struct Out {
    ExecutionGateMode effective_mode;

    template <StructureFormatter F> void Format(F &fmt) {
      fmt.Value("{}", effective_mode);
    }

    Out(ExecutionGateMode /*mode*/) {}
  };
};

using ExecutionGateAction = Action<SetExecutionGateMode>;

} // namespace edr

template <> struct std::formatter<edr::ExecutionGateMode, char> {
  bool reverse = false;

  template <class ParseContext>
  constexpr ParseContext::iterator parse(ParseContext &ctx) {
    auto it = ctx.begin();
    while (*it != '}')
      it++;

    return it;
  }

  template <class FmtContext>
  FmtContext::iterator format(edr::ExecutionGateMode &mode,
                              FmtContext &ctx) const {
    switch (mode) {
    case edr::ExecutionGateMode::ForceStallIfNoRequsts:
      return std::format_to(ctx.out(), "Always stall if no pending requests");

    case edr::ExecutionGateMode::ForceStall:
      return std::format_to(ctx.out(), "Force stall");

    case edr::ExecutionGateMode::NoStall:
      return std::format_to(ctx.out(), "No stall");

    case edr::ExecutionGateMode::Terminated:
      return std::format_to(ctx.out(), "Terminated");

    default:
      return std::format_to(
          ctx.out(), "Stall if no pending requests and the target is idle");
    }
  }
};

#endif
