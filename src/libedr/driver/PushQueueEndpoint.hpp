#ifndef LIBEDR_DRIVER_PUSHQUEUEENDPOINT_HPP
#define LIBEDR_DRIVER_PUSHQUEUEENDPOINT_HPP

#include "libedr/util/asynchronicity/ResolutionQueue.hpp"

#include <condition_variable>
#include <mutex>

namespace edr {

template <class TItem> class PushQueueEndpoint {
public:
  using TransactionQueue = ResolutionQueue<TItem>;
  using Item = TransactionQueue::Item;

  class Enqueuer {
  public:
    Enqueuer(PushQueueEndpoint<TItem> &owner)
        : m_owner(owner), m_lock(owner.m_mutex) {}

    auto Enqueue(Item &item) {
      auto pending_task = m_owner.m_queue.Enqueue(item);
      m_lock.unlock();

      m_owner.m_queue_updated.notify_all();

      return pending_task;
    }

  private:
    PushQueueEndpoint<TItem> &m_owner;
    std::unique_lock<std::mutex> m_lock;
  };

  Enqueuer LockForEnqueue() { return Enqueuer(*this); }

  template <class F> bool Serve(bool wait_if_empty, F &&handle_transaction) {
    bool pending_notification = false;

    std::unique_lock<std::mutex> lock(m_mutex);
    if (wait_if_empty && m_queue.NoScheduled())
      m_queue_updated.wait(lock);

    while (!m_queue.NoScheduled() || m_transaction_in_progress) {
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
      if (m_queue.NoScheduled() || m_transaction_in_progress) {
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
    m_queue.StartNextScheduled();
    auto current = m_queue.PrepareToResolve();
    m_transaction_in_progress = true;

    lock.unlock();

    if (pending_notification) {
      m_queue_updated.notify_all();
      pending_notification = false;
    }

    handle_transaction(current.Data());
    current.Resolve();

    lock.lock();
    m_transaction_in_progress = false;
    pending_notification = true;
  }

  std::mutex m_mutex;

  TransactionQueue m_queue;

  std::condition_variable m_queue_updated;
  bool m_transaction_in_progress = false;
  bool m_is_alive = true;
};

} // namespace edr

#endif
