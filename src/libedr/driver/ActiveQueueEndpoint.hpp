#ifndef LIBEDR_DRIVER_ACTIVEQUEUEENDPOINT_HPP
#define LIBEDR_DRIVER_ACTIVEQUEUEENDPOINT_HPP

#include "libedr/util/asynchronicity/AsynchronousPrimitives.hpp"

#include <condition_variable>
#include <mutex>

namespace edr {

template <class TAsynchronous, class TItem> class ActiveQueueEndpoint {
public:
  ActiveQueueEndpoint(TAsynchronous &owner) : m_queue(owner) {}

  class Enqueuer {
  public:
    Enqueuer(ActiveQueueEndpoint<TAsynchronous, TItem> &owner)
        : m_owner(owner), m_lock(owner.m_mutex) {}

    template <class... Args> auto Enqueue(Args &&...args) {
      auto pending_task = m_owner.m_queue.Emplace(std::forward<Args>(args)...);
      m_lock.unlock();

      m_owner.m_queue_updated.notify_all();

      return pending_task;
    }

  private:
    ActiveQueueEndpoint<TAsynchronous, TItem> &m_owner;
    std::unique_lock<std::mutex> m_lock;
  };

  Enqueuer LockForEnqueue() { return Enqueuer(*this); }

  template <class F> bool Serve(bool wait_if_empty, F &&handle_transaction) {
    bool pending_notification = false;

    std::unique_lock<std::mutex> lock(m_mutex);
    if (wait_if_empty && m_queue.Empty())
      m_queue_updated.wait(lock);

    while (!m_queue.Empty() || m_transaction_in_progress) {
      if (m_transaction_in_progress) {
        m_queue_updated.wait(lock);
        continue;
      }

      ServeFront(lock, pending_notification,
                 std::forward<F>(handle_transaction));
    }

    lock.unlock();
    if (pending_notification)
      m_queue_updated.notify_all();

    return m_is_alive;
  }

  template <class F> void Terminate(F &&handle_transaction) {
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_is_alive = false;
    }

    m_queue_updated.notify_all();

    Serve(false, std::forward<F>(handle_transaction));
  }

  template <class F>
  void Join(const std::coroutine_handle<> &to_complete,
            F &&handle_transaction) {
    if (!to_complete)
      return;

    bool pending_notification = false;

    std::unique_lock<std::mutex> lock(m_mutex);
    while (!to_complete.done()) {
      if (m_queue.Empty() || m_transaction_in_progress) {
        m_queue_updated.wait(lock);
        continue;
      }

      ServeFront(lock, pending_notification,
                 std::forward<F>(handle_transaction));
    }

    lock.unlock();
    if (pending_notification)
      m_queue_updated.notify_all();
  }

private:
  template <class F>
  void ServeFront(std::unique_lock<std::mutex> &lock,
                  bool &pending_notification, F &&handle_transaction) {
    auto current = m_queue.Pop();
    m_transaction_in_progress = true;

    lock.unlock();

    if (pending_notification) {
      m_queue_updated.notify_all();
      pending_notification = false;
    }

    handle_transaction(*current);
    current.Resolve();

    lock.lock();
    m_transaction_in_progress = false;
    pending_notification = true;
  }

  std::mutex m_mutex;

  ResolutionQueue<TAsynchronous, TItem> m_queue;

  std::condition_variable m_queue_updated;
  bool m_transaction_in_progress = false;
  bool m_is_alive = true;
};

} // namespace edr

#endif
