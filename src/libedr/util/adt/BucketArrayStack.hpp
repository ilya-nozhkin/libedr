#ifndef LIBEDR_UTIL_ADT_BUCKETARRAYSTACK_HPP
#define LIBEDR_UTIL_ADT_BUCKETARRAYSTACK_HPP

#include "libedr/util/adt/BucketArrayMap.hpp"

#include <memory>

namespace edr {

template <class T, size_t t_num_items_in_bucket,
          class TAllocator = std::allocator<std::byte>>
class BucketArrayStack {
public:
  BucketArrayStack(const TAllocator &allocator = {}) : m_map(allocator) {}

  bool Empty() { return 0 == m_head_index; }

  template <class... Args> T *Emplace(Args &&...args) {
    auto *emplaced = m_map.Emplace(m_head_index, std::forward<Args>(args)...);
    if (nullptr == emplaced)
      return nullptr;

    m_head_index++;
    return emplaced;
  }

  T Pop() {
    assert(!Empty());

    m_head_index--;
    return m_map.TakeExisting(m_head_index);
  }

private:
  BucketArrayMap<T, t_num_items_in_bucket, TAllocator> m_map;
  size_t m_head_index = 0;
};

} // namespace edr

#endif
