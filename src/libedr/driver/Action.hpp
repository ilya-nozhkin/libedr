#ifndef LIBEDR_DRIVER_ACTION_HPP
#define LIBEDR_DRIVER_ACTION_HPP

#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/util/miscellaneous/Formatting.hpp"
#include "libedr/util/vss/VSSPayloads.hpp"

#include <concepts>
#include <format>
#include <type_traits>

namespace edr {

template <class T>
concept ActionType =
    StructurallyFormattable<T> && std::is_trivially_destructible_v<T> &&
    std::is_trivially_destructible_v<typename T::Out> && requires() {
      { T::g_id } -> std::convertible_to<ActionID>;
    };

struct ActionIDGetter {
  template <class A> consteval ActionID operator()() { return A::g_id; }
};

template <ActionType... TActionTypes>
using Action = vss::VariantBase<ActionID, ActionIDGetter, TActionTypes...>;

template <class T>
concept IsAction = requires(T action) {
  []<class... TActionTypes>(const Action<TActionTypes...> &) {}(action);
};

} // namespace edr

template <> struct std::formatter<edr::ActionID, char> {
  template <class ParseContext>
  constexpr ParseContext::iterator parse(ParseContext &ctx) {
    auto it = ctx.begin();
    while (*it != '}')
      it++;

    return it;
  }

  template <class FmtContext>
  FmtContext::iterator format(const edr::ActionID &id, FmtContext &ctx) const {
    return std::format_to(ctx.out(), "{}", static_cast<uint32_t>(id));
  }
};

template <edr::ActionType... TActionTypes>
struct std::formatter<edr::Action<TActionTypes...>, char> {
  template <class ParseContext>
  constexpr ParseContext::iterator parse(ParseContext &ctx) {
    auto it = ctx.begin();
    while (*it != '}')
      it++;

    return it;
  }

  template <class FmtContext>
  FmtContext::iterator format(edr::Action<TActionTypes...> &act,
                              FmtContext &ctx) const {
    if (!act.IsValid())
      return std::format_to(ctx.out(), "Action {}", act.discriminant);

    auto it = ctx.out();
    act.Visit([&it](auto &action) { it = std::format_to(it, "{}", action); });
    return it;
  }
};

#endif
