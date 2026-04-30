#ifndef LIBEDR_UTIL_ASYNCHRONICITY_ASYNCGATE_HPP
#define LIBEDR_UTIL_ASYNCHRONICITY_ASYNCGATE_HPP

#include "libedr/util/asynchronicity/ResolutionQueue.hpp"

#include <cassert>
#include <mutex>

namespace edr {

class AsyncGate {
public:
  struct Data {};
  using Queue = ResolutionQueue<Data>;
  using Item = Queue::Item;

  struct Awaitable {
  public:
    Awaitable(std::unique_lock<std::mutex> &m_lock) : m_lock(m_lock) {}

    Awaitable(std::unique_lock<std::mutex> &m_lock, Queue::Awaitable &&queued)
        : m_lock(m_lock), m_queued(std::move(queued)) {}

    bool await_ready() const noexcept { return !m_queued.has_value(); }

    bool await_suspend(std::coroutine_handle<> continuation) noexcept {
      m_lock.unlock();
      return m_queued->await_suspend(continuation);
    }

    void await_resume() {
      if (m_queued.has_value())
        m_lock.lock();
    }

  private:
    std::unique_lock<std::mutex> &m_lock;
    std::optional<Queue::Awaitable> m_queued;
  };

  Awaitable Pass(std::unique_lock<std::mutex> &lock, Item &item) {
    if (m_is_open)
      return Awaitable(lock);

    return Awaitable(lock, m_queue.Enqueue(item));
  }

  void Close(std::unique_lock<std::mutex> & /*lock*/) {
    assert(m_is_open);
    m_is_open = false;
  }

  void Open(std::unique_lock<std::mutex> &lock) {
    assert(!m_is_open);

    while (!m_queue.NoScheduled()) {
      m_queue.StartNextScheduled();
      auto resolver = m_queue.PrepareToResolve();

      lock.unlock();
      resolver.Resolve();
      lock.lock();
    }

    m_is_open = true;
  }

private:
  bool m_is_open = true;
  Queue m_queue;
};

} // namespace edr

#endif
