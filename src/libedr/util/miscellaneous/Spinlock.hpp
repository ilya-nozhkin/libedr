#ifndef LIBEDR_UTIL_MISCELLANEOUS_SPINLOCK_HPP
#define LIBEDR_UTIL_MISCELLANEOUS_SPINLOCK_HPP

#include <atomic>

namespace edr {

class Spinlock {
public:
  void Lock() {
    while (flag.test_and_set(std::memory_order_acquire))
      ;
  }

  void Unlock() { flag.clear(std::memory_order_release); }

private:
  std::atomic_flag flag = {};
};

class SpinlockGuard {
public:
  SpinlockGuard(Spinlock &spinlock) : m_spinlock(&spinlock) { spinlock.Lock(); }

  SpinlockGuard(SpinlockGuard &&guard) : m_spinlock(guard.m_spinlock) {
    guard.m_spinlock = nullptr;
  }

  SpinlockGuard(const SpinlockGuard &) = delete;
  SpinlockGuard &operator=(const SpinlockGuard &) = delete;
  SpinlockGuard &operator=(SpinlockGuard &&) = delete;

  ~SpinlockGuard() {
    if (nullptr != m_spinlock)
      m_spinlock->Unlock();
  }

  void Release() {
    if (nullptr != m_spinlock)
      m_spinlock->Unlock();

    m_spinlock = nullptr;
  }

private:
  Spinlock *m_spinlock;
};

} // namespace edr

#endif
