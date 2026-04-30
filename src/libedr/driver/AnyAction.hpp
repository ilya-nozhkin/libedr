#ifndef LIBEDR_DRIVER_ANYACTION_HPP
#define LIBEDR_DRIVER_ANYACTION_HPP

#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/driver/apb/APBAction.hpp"
#include "libedr/driver/bytestream/ByteStreamAction.hpp"
#include "libedr/driver/execution_gate/ExecutionGateAction.hpp"
#include "libedr/driver/jtag/JtagAction.hpp"
#include "libedr/driver/jtag/JtagChainAction.hpp"
#include "libedr/driver/riscv/RVDTMAction.hpp"

namespace edr {

namespace {

template <IsAction... TActions> struct JoinActions;

template <ActionType... THeadTypes, IsAction... TTail>
struct JoinActions<Action<THeadTypes...>, TTail...> {
  template <class... Ts>
  using Type = JoinActions<TTail...>::template Type<Ts..., THeadTypes...>;
};

template <> struct JoinActions<> {
  template <class... Ts> using Type = Action<Ts...>;
};

template <DriverID t_id, IsAction TAction> struct Pair {
  static inline constexpr DriverID g_id = t_id;
  using Action = TAction;
};

} // namespace

template <class... TPairs> struct ActionByDriverImpl {
  using AnyAction = JoinActions<typename TPairs::Action...>::template Type<>;

  template <class F> static void ForID(DriverID id, F &&func) {
    (
        [&]() {
          if (id == TPairs::g_id)
            func.template operator()<TPairs::g_id, typename TPairs::Action>();
        }(),
        ...);
  }
};

// DEFINE YOUR DRIVER HERE
// ===========================
using ActionByDriver =
    ActionByDriverImpl<Pair<DriverID::ByteStream, ByteStreamAction>,
                       Pair<DriverID::Jtag, JtagAction>,
                       Pair<DriverID::APB, APBAction>,
                       Pair<DriverID::JtagChain, JtagChainAction>,
                       Pair<DriverID::RVDTM, RVDTMAction>,
                       Pair<DriverID::ExecutionGate, ExecutionGateAction>>;
// ===========================

using AnyAction = ActionByDriver::AnyAction;

} // namespace edr

#endif
