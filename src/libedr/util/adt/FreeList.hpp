#ifndef LIBEDR_UTIL_ADT_FREELIST_HPP
#define LIBEDR_UTIL_ADT_FREELIST_HPP

#include <cassert>
#include <memory_resource>

namespace edr {

template <class T> class FreeList {
public:
  FreeList(std::pmr::memory_resource &resource) : m_resource(resource) {}

  ~FreeList() {
    while (nullptr != empty_head) {
      Item *to_delete = empty_head;
      empty_head = empty_head->next;
      m_resource.deallocate(to_delete, sizeof(Item));
    }

    while (nullptr != full_head) {
      Item *to_delete = full_head;
      full_head = full_head->next;
      to_delete->value.~T();
      m_resource.deallocate(to_delete, sizeof(Item));
    }
  }

  template <class... Args> T TakeOrMake(Args &&...args) {
    if (nullptr == full_head)
      return T(std::forward<Args>(args)...);

    Item *taken = full_head;
    full_head = full_head->next;

    T result(std::move(taken->value));
    taken->value.~T();

    taken->next = empty_head;
    empty_head = taken;

    return result;
  }

  void Free(T &&value) {
    Item *item = nullptr;
    if (nullptr != empty_head) {
      item = empty_head;
      empty_head = empty_head->next;

      item->next = full_head;
      new (&item->value) T(std::move(value));
    } else {
      auto *ptr = m_resource.allocate(sizeof(Item));
      if (nullptr == ptr)
        return;

      item = new (ptr) Item{.next = full_head, .value = std::move(value)};
    }

    full_head = item;
  }

private:
  struct Item {
    Item *next;
    T value;
  };

  std::pmr::memory_resource &m_resource;
  Item *empty_head = nullptr;
  Item *full_head = nullptr;
};

} // namespace edr

#endif
