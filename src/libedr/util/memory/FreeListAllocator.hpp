#ifndef LIBEDR_UTIL_MEMORY_FREELISTALLOCATOR_HPP
#define LIBEDR_UTIL_MEMORY_FREELISTALLOCATOR_HPP

#include "libedr/util/miscellaneous/Spinlock.hpp"

#include <cassert>
#include <cstddef>
#include <memory_resource>

namespace edr {

template <class T> class FreeListAllocator {
public:
  FreeListAllocator(std::pmr::memory_resource &resource)
      : m_resource(resource) {}

  FreeListAllocator(const FreeListAllocator &from)
      : m_resource(from.m_resource) {}
  FreeListAllocator(FreeListAllocator &&from)
      : m_resource(from.m_resource), m_head(from.m_head) {
    from.m_head = nullptr;
  }

  FreeListAllocator &operator=(const FreeListAllocator &) = delete;
  FreeListAllocator &operator=(FreeListAllocator &&) = delete;

  ~FreeListAllocator() {
    while (nullptr != m_head) {
      Header *to_delete = m_head;
      m_head = m_head->next;
      m_resource.deallocate(to_delete, to_delete->total_size);
    }
  }

  T *allocate(size_t num) {
    size_t total_size = sizeof(Header) + num * sizeof(T);

    auto allocate_new = [this, total_size]() -> T * {
      void *ptr = m_resource.allocate(total_size);
      if (nullptr == ptr)
        return nullptr;

      Header *chunk =
          new (ptr) Header{.total_size = total_size, .next = nullptr};
      return reinterpret_cast<T *>(chunk + 1);
    };

    Header *taken = nullptr;

    {
      SpinlockGuard guard(m_spinlock);
      if (nullptr == m_head) {
        guard.Release();
        return allocate_new();
      }

      taken = m_head;
      m_head = m_head->next;
    }

    if (taken->total_size < total_size) {
      m_resource.deallocate(taken, taken->total_size);
      return allocate_new();
    }

    return reinterpret_cast<T *>(taken + 1);
  }

  void deallocate(void *ptr, size_t num) {
    Header *chunk = reinterpret_cast<Header *>(ptr) - 1;
    assert(chunk->total_size >= sizeof(Header) + num * sizeof(T));

    SpinlockGuard guard(m_spinlock);
    chunk->next = m_head;
    m_head = chunk;
  }

private:
  struct alignas(alignof(std::max_align_t)) Header {
    size_t total_size;
    Header *next;
  };

  Spinlock m_spinlock;
  std::pmr::memory_resource &m_resource;
  Header *m_head = nullptr;
};

} // namespace edr

#endif
