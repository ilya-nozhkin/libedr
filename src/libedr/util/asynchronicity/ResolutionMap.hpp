#ifndef LIBEDR_UTIL_ASYNCHRONICITY_RESOLUTIONMAP_HPP
#define LIBEDR_UTIL_ASYNCHRONICITY_RESOLUTIONMAP_HPP

#include "libedr/util/adt/BucketArrayMap.hpp"
#include "libedr/util/asynchronicity/Asynchronicity.hpp"

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <utility>

namespace edr {

template <class TAsynchronous, size_t t_num_items_in_bucket, class T,
          class... Results>
class ResolutionMap {
public:
  ResolutionMap(TAsynchronous &owner)
      : m_owner(owner), m_map(owner.GetFrameAllocator()) {}

  struct Item {
    template <class... Args>
    Item(Args &&...args) : data(std::forward<Args>(args)...) {}

    EagerResolver<Results...> resolver;
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
