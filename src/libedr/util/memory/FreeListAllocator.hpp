#ifndef LIBEDR_UTIL_MEMORY_FREELISTALLOCATOR_HPP
#define LIBEDR_UTIL_MEMORY_FREELISTALLOCATOR_HPP

#include "libedr/util/asynchronicity/Asynchronicity.hpp"
#include "libedr/util/miscellaneous/Spinlock.hpp"

#include <cassert>
#include <cstddef>
#include <memory_resource>

namespace edr {

class FreeListAllocatorResource {
public:
  FreeListAllocatorResource(std::pmr::memory_resource &resource)
      : m_resource(resource) {}

  FreeListAllocatorResource(const FreeListAllocatorResource &from)
      : m_resource(from.m_resource) {}

  FreeListAllocatorResource(FreeListAllocatorResource &&from)
      : m_resource(from.m_resource), m_head(from.m_head) {
    from.m_head = nullptr;
  }

  FreeListAllocatorResource &
  operator=(const FreeListAllocatorResource &) = delete;
  FreeListAllocatorResource &operator=(FreeListAllocatorResource &&) = delete;

  ~FreeListAllocatorResource() {
    while (nullptr != m_head) {
      Header *to_delete = m_head;
      m_head = m_head->next;
      m_resource.deallocate(to_delete, to_delete->total_size);
    }
  }

  void *allocate(size_t num_bytes) {
    size_t total_size = sizeof(Header) + num_bytes;

    auto allocate_new = [this, total_size]() -> void * {
      void *ptr = m_resource.allocate(total_size);
      if (nullptr == ptr)
        return nullptr;

      Header *chunk =
          new (ptr) Header{.total_size = total_size, .next = nullptr};
      return chunk + 1;
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

    return taken + 1;
  }

  void deallocate(void *ptr, size_t num_bytes) {
    Header *chunk = reinterpret_cast<Header *>(ptr) - 1;
    assert(chunk->total_size >= sizeof(Header) + num_bytes);

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

template <class T> class FreeListAllocator {
  template <class U> friend class FreeListAllocator;

public:
  using value_type = T;

  FreeListAllocator(FreeListAllocatorResource &resource)
      : m_resource(resource) {}

  template <class U>
  FreeListAllocator(const FreeListAllocator<U> &from)
      : m_resource(from.m_resource) {}

  FreeListAllocator(const FreeListAllocator &from)
      : m_resource(from.m_resource) {}

  FreeListAllocator(FreeListAllocator &&from) : m_resource(from.m_resource) {}

  FreeListAllocator &operator=(const FreeListAllocator &) = delete;
  FreeListAllocator &operator=(FreeListAllocator &&) = delete;

  ~FreeListAllocator() = default;

  T *allocate(size_t num) {
    return reinterpret_cast<T *>(m_resource.allocate(num * sizeof(T)));
  }

  void deallocate(void *ptr, size_t num) {
    return m_resource.deallocate(ptr, num * sizeof(T));
  }

private:
  FreeListAllocatorResource &m_resource;
};

using ByteFreeListAllocator = FreeListAllocator<std::byte>;

class FreeListAsynchronousResourceStorage {
public:
  FreeListAsynchronousResourceStorage(std::pmr::memory_resource &resource)
      : m_resource(resource) {}

protected:
  FreeListAllocatorResource m_resource;
};

class FreeListAsynchronous : private FreeListAsynchronousResourceStorage,
                             public Asynchronous<ByteFreeListAllocator> {
public:
  FreeListAsynchronous(std::pmr::memory_resource &resource)
      : FreeListAsynchronousResourceStorage(resource),
        Asynchronous(m_resource) {}
};

} // namespace edr

#endif
