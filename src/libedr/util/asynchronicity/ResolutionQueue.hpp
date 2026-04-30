#ifndef LIBEDR_UTIL_ASYNCHRONICITY_RESOLUTIONQUEUE_HPP
#define LIBEDR_UTIL_ASYNCHRONICITY_RESOLUTIONQUEUE_HPP

#include "libedr/util/asynchronicity/Asynchronicity.hpp"

#include <cassert>
#include <cstdlib>
#include <utility>

namespace edr {

template <class T, class... Results> class ResolutionQueue {
public:
  class Awaitable;
  class Resolvable;

  class Item {
    friend class ResolutionQueue;
    friend class ResolutionQueue::Awaitable;
    friend class ResolutionQueue::Resolvable;

  public:
    template <class... Args>
    Item(Args &&...args) : m_data(std::forward<Args>(args)...) {}

    Item(const Item &) = delete;
    Item(Item &&) = delete;
    Item &operator=(const Item &) = delete;
    Item &operator=(Item &&) = delete;

    T *operator->() { return &m_data; }

    T &Data() { return m_data; }

  private:
    Item *m_next = nullptr;
    EagerResolver<Results...> m_resolver = {};
    T m_data;
  };

  class Awaitable : public AwaitableBase<Awaitable, Results...> {
  public:
    Awaitable(Item &item) : m_item(&item) {}

    ~Awaitable() {
      if (nullptr == m_item)
        return;

      auto previous_state = m_item->m_resolver.m_continuation.exchange(
          g_task_state_awaitable_destroyed);

      assert(g_task_state_result_ready == previous_state);
    }

    Awaitable(Awaitable &&from) : m_item(from.m_item) { from.m_item = nullptr; }

    Awaitable(const Awaitable &) = delete;
    Awaitable &operator=(const Awaitable &) = delete;
    Awaitable &operator=(Awaitable &&) = delete;

    template <class F> auto ForResolver(F &&func) {
      return std::forward<F>(func)(m_item->m_resolver);
    }

    template <class F> auto ForResolver(F &&func) const {
      return std::forward<F>(func)(m_item->m_resolver);
    }

    operator bool() const { return nullptr != m_item; }

  private:
    Item *m_item;
  };

  class Resolvable {
  public:
    Resolvable(Item &item) : m_item(&item) {}

    Resolvable(Resolvable &&from) : m_item(from.m_item) {
      from.m_item = nullptr;
    }

    Resolvable(const Resolvable &) = delete;
    Resolvable &operator=(const Resolvable &) = delete;
    Resolvable &operator=(Resolvable &&) = delete;

    T *operator->() { return &m_item->m_data; }

    T &Data() { return m_item->m_data; }

    template <class... Args> void Resolve(Args &&...args) {
      if constexpr (0 == sizeof...(Args))
        m_item->m_resolver.return_void(std::forward<Args>(args)...);
      else
        m_item->m_resolver.return_value(std::forward<Args>(args)...);
    }

  private:
    Item *m_item;
  };

  Awaitable Enqueue(Item &item) {
    if (m_scheduled_head == nullptr) {
      m_scheduled_head = &item;
      m_scheduled_last = &item;
    } else {
      m_scheduled_last->m_next = &item;
      m_scheduled_last = &item;
    }

    return Awaitable(item);
  }

  bool NoScheduled() { return nullptr == m_scheduled_head; }

  bool NoInProgress() { return nullptr == m_in_progress_head; }

  Item &StartNextScheduled() {
    assert(!NoScheduled());

    auto *item = m_scheduled_head;
    m_scheduled_head = m_scheduled_head->m_next;

    if (nullptr == m_in_progress_head) {
      m_in_progress_head = item;
      m_in_progress_last = item;
    } else {
      m_in_progress_last->m_next = item;
      m_in_progress_last = item;
    }

    return *item;
  }

  Item &GetNextScheduled() {
    assert(!NoScheduled());

    return *m_scheduled_head;
  }

  Item &GetCurrentInProgress() {
    assert(!NoInProgress());

    return *m_in_progress_head;
  }

  Resolvable PrepareToResolve() {
    assert(!NoInProgress());

    auto *item = m_in_progress_head;
    m_in_progress_head = m_in_progress_head->m_next;
    return Resolvable(*item);
  }

private:
  Item *m_scheduled_head = nullptr;
  Item *m_scheduled_last = nullptr;
  Item *m_in_progress_head = nullptr;
  Item *m_in_progress_last = nullptr;
};

} // namespace edr

#endif
