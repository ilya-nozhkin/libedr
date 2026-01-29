#ifndef LIBEDR_UTIL_MISCELLANEOUS_SCOPEGUARD_HPP
#define LIBEDR_UTIL_MISCELLANEOUS_SCOPEGUARD_HPP

#include <type_traits>
#include <utility>

namespace edr {

template <class F> class ScopeGuard {
public:
  template <class G> ScopeGuard(G &&func) : m_func(std::forward<G>(func)) {}

  ScopeGuard(const ScopeGuard &) = delete;
  ScopeGuard(ScopeGuard &&) = delete;
  ScopeGuard &operator=(const ScopeGuard &) = delete;
  ScopeGuard &operator=(ScopeGuard &&) = delete;

  ~ScopeGuard() {
    if (m_engaged)
      m_func();
  }

  void Release() { m_engaged = false; }

private:
  F m_func;
  bool m_engaged = true;
};

template <class F>
ScopeGuard<std::remove_reference_t<F>> MakeScopeGuard(F &&func) {
  return ScopeGuard<std::remove_reference_t<F>>(std::forward<F>(func));
}

} // namespace edr

#endif
