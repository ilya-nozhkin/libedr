#ifndef LIBEDR_UTIL_MISCELLANEOUS_OVERLOAD_HPP
#define LIBEDR_UTIL_MISCELLANEOUS_OVERLOAD_HPP

namespace edr {

template <class... Ts> struct Overload : Ts... {
  using Ts::operator()...;
};

template <class... Ts> Overload(Ts...) -> Overload<Ts...>;

} // namespace edr

#endif
