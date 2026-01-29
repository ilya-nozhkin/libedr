#ifndef LIBEDR_UTIL_ASYNCHRONICITY_ASYNCHRONOUSPRIMITIVES_HPP
#define LIBEDR_UTIL_ASYNCHRONICITY_ASYNCHRONOUSPRIMITIVES_HPP

#include "libedr/util/adt/BucketArrayMap.hpp"
#include "libedr/util/asynchronicity/Asynchronicity.hpp"

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <utility>

namespace edr {

template <class TAsynchronous, class T, class... Results>
class ResolutionQueue {
public:
  struct Item {
    Item *next = nullptr;
    Resolver<Results...> resolver = {};
    T data;

    template <class... Args>
    Item(Args &&...args) : data(std::forward<Args>(args)...) {}
  };

  class ItemInProgress {
  public:
    ItemInProgress(TAsynchronous &owner, Item *item)
        : m_owner(owner), m_item(item) {}

    ItemInProgress(ItemInProgress &&from)
        : m_owner(from.m_owner), m_item(from.m_item),
          m_resolved(from.m_resolved) {
      from.m_item = nullptr;
    }

    ItemInProgress(const ItemInProgress &) = delete;
    ItemInProgress &operator=(const ItemInProgress &) = delete;
    ItemInProgress &operator=(ItemInProgress &&) = delete;

    ~ItemInProgress() {
      if (nullptr != m_item) {
        assert(m_resolved);

        m_item->~Item();
        m_owner.GetFrameAllocator().deallocate(
            reinterpret_cast<std::byte *>(m_item), sizeof(Item));
      }
    }

    T *operator->() { return &m_item->data; }

    T &operator*() { return m_item->data; }

    template <class... Args> void Resolve(Args &&...args) {
      auto previous_state = g_task_state_initial;
      if constexpr (0 == sizeof...(Args))
        previous_state =
            m_item->resolver.return_void(std::forward<Args>(args)...);
      else
        previous_state =
            m_item->resolver.return_value(std::forward<Args>(args)...);

      // If the awaitable was alive, then it is responsible for managing the
      // item's lifetime.
      if (g_task_state_awaitable_destroyed != previous_state)
        m_item = nullptr;

      m_resolved = true;
    }

  private:
    TAsynchronous &m_owner;
    Item *m_item;
    bool m_resolved = false;
  };

  class Awaitable : public AwaitableBase<Awaitable, Results...> {
  public:
    Awaitable(TAsynchronous &owner, Item *item)
        : m_owner(owner), m_item(item) {}

    ~Awaitable() {
      if (nullptr == m_item)
        return;

      auto previous_state = m_item->resolver.m_continuation.exchange(
          g_task_state_awaitable_destroyed);
      if (g_task_state_initial == previous_state)
        return;

      assert(g_task_state_result_ready == previous_state);
      m_item->~Item();
      m_owner.GetFrameAllocator().deallocate(
          reinterpret_cast<std::byte *>(m_item), sizeof(Item));
    }

    Awaitable(Awaitable &&from) : m_owner(from.m_owner), m_item(from.m_item) {
      from.m_item = nullptr;
    }

    Awaitable(const Awaitable &) = delete;
    Awaitable &operator=(const Awaitable &) = delete;
    Awaitable &operator=(Awaitable &&) = delete;

    template <class F> auto ForResolver(F &&func) {
      return std::forward<F>(func)(m_item->resolver);
    }

    template <class F> auto ForResolver(F &&func) const {
      return std::forward<F>(func)(m_item->resolver);
    }

    operator bool() const { return nullptr != m_item; }

  private:
    TAsynchronous &m_owner;
    Item *m_item;
  };

  ResolutionQueue(TAsynchronous &owner) : m_owner(owner) {}

  template <class... Args> Awaitable Emplace(Args &&...args) {
    void *new_tail_ptr = m_owner.GetFrameAllocator().allocate(sizeof(Item));
    if (nullptr == new_tail_ptr)
      return Awaitable(m_owner, nullptr);

    Item *new_tail = new (new_tail_ptr) Item(std::forward<Args>(args)...);

    if (nullptr == m_tail) {
      assert(nullptr == m_head);
      m_head = m_tail = new_tail;
    } else {
      m_tail->next = new_tail;
      m_tail = new_tail;
    }

    return Awaitable(m_owner, new_tail);
  }

  bool Empty() { return nullptr == m_head; }

  ItemInProgress Pop() {
    Item *old_head = m_head;
    m_head = old_head->next;
    if (nullptr == m_head)
      m_tail = nullptr;

    return ItemInProgress(m_owner, old_head);
  }

  T &Front() { return m_head->data; }

private:
  TAsynchronous &m_owner;

  Item *m_head = nullptr;
  Item *m_tail = nullptr;
};

template <class TAsynchronous, size_t t_num_items_in_bucket, class T,
          class... Results>
class ResolutionMap {
public:
  ResolutionMap(TAsynchronous &owner)
      : m_owner(owner), m_map(owner.GetFrameAllocator()) {}

  struct Item {
    template <class... Args>
    Item(Args &&...args) : data(std::forward<Args>(args)...) {}

    Resolver<Results...> resolver;
    T data;
  };

  class Awaitable : public AwaitableBase<Awaitable, Results...> {
  public:
    Awaitable(Item *item) : m_item(item) {}

    ~Awaitable() {
      if (nullptr == m_item)
        return;

      m_item->resolver.m_continuation.exchange(
          g_task_state_awaitable_destroyed);
    }

    Awaitable(Awaitable &&from) : m_item(from.m_item) { from.m_item = nullptr; }

    Awaitable(const Awaitable &) = delete;
    Awaitable &operator=(const Awaitable &) = delete;
    Awaitable &operator=(Awaitable &&) = delete;

    template <class F> auto ForResolver(F &&func) {
      return std::forward<F>(func)(m_item->resolver);
    }

    template <class F> auto ForResolver(F &&func) const {
      return std::forward<F>(func)(m_item->resolver);
    }

    operator bool() const { return nullptr != m_item; }

    TaskResultHelper<Results...>::Type TakeFallbackResult() { return {}; }

  private:
    Item *m_item;
  };

  template <class... Args> Awaitable Emplace(size_t index, Args &&...args) {
    auto *emplaced = m_map.Emplace(index, std::forward<Args>(args)...);
    return Awaitable(emplaced);
  }

  template <class... Args> bool Resolve(size_t index, Args &&...args) {
    Item *item = m_map.Get(index);
    if (nullptr == item)
      return false;

    if constexpr (0 == sizeof...(Args))
      item->resolver.return_void(std::forward<Args>(args)...);
    else
      item->resolver.return_value(std::forward<Args>(args)...);

    m_map.Erase(index);
  }

  Item *Get(size_t index) { return m_map.Get(index); }

  void Erase(size_t index) { m_map.Erase(index); }

  auto begin() { return m_map.begin(); }

  auto end() { return m_map.end(); }

private:
  TAsynchronous &m_owner;
  BucketArrayMap<Item, t_num_items_in_bucket,
                 typename TAsynchronous::FrameAllocator>
      m_map;
};

} // namespace edr

#endif
