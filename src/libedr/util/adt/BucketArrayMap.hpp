#ifndef LIBEDR_UTIL_ADT_BUCKETARRAYMAP_HPP
#define LIBEDR_UTIL_ADT_BUCKETARRAYMAP_HPP

#include <cassert>
#include <memory>
#include <optional>

namespace edr {

template <class T, size_t t_num_items_in_bucket,
          class TAllocator = std::allocator<std::byte>>
class BucketArrayMap {
  struct Bucket {
    std::optional<T> items[t_num_items_in_bucket];
    Bucket *next = nullptr;
  };

  using Allocator =
      std::allocator_traits<TAllocator>::template rebind_alloc<Bucket>;

public:
  class Iterator {
  public:
    Iterator(Bucket *bucket, size_t offset)
        : m_bucket(bucket), m_offset(offset) {}

    T &operator*() const { return *m_bucket->items[m_offset]; }

    T *operator->() const { return &*m_bucket->items[m_offset]; }

    Iterator &operator++() {
      while (nullptr != m_bucket) {
        m_offset++;

        if (m_offset == t_num_items_in_bucket) {
          m_offset = 0;
          m_bucket = m_bucket->next;
          if (nullptr == m_bucket)
            return *this;
        }

        if (m_bucket->items[m_offset].has_value())
          break;
      }

      return *this;
    }

    Iterator operator++(int) {
      auto tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(const Iterator &it) const {
      return m_bucket == it.m_bucket && m_offset == it.m_offset;
    }

  private:
    Bucket *m_bucket;
    size_t m_offset;
  };

  BucketArrayMap(const TAllocator &allocator = {}) : m_allocator(allocator) {}

  BucketArrayMap(BucketArrayMap &&from)
      : m_allocator(std::move(from.m_allocator)), m_head(from.m_head) {
    from.m_head = nullptr;
  }

  BucketArrayMap(const BucketArrayMap &) = delete;
  BucketArrayMap &operator=(const BucketArrayMap &) = delete;
  BucketArrayMap &operator=(BucketArrayMap &&) = delete;

  ~BucketArrayMap() {
    while (nullptr != m_head) {
      Bucket *to_delete = m_head;
      m_head = m_head->next;

      to_delete->~Bucket();
      m_allocator.deallocate(to_delete, 1);
    }
  }

  Iterator begin() {
    auto it = Iterator(m_head, 0);
    if (nullptr != m_head && !m_head->items[0].has_value())
      it++;

    return it;
  }

  Iterator end() { return Iterator(nullptr, 0); }

  template <class... Args> T *Emplace(size_t index, Args &&...args) {
    auto [bucket, offset] = GetBucketAndOffset(index, true);
    if (nullptr == bucket)
      return nullptr;

    auto &item = bucket->items[offset].emplace(std::forward<Args>(args)...);
    return &item;
  }

  T *Get(size_t index) {
    auto [bucket, offset] = GetBucketAndOffset(index, false);
    if (nullptr == bucket)
      return nullptr;

    auto &mb_item = bucket->items[offset];
    if (!mb_item)
      return nullptr;

    return &*mb_item;
  }

  void Erase(size_t index) {
    auto [bucket, offset] = GetBucketAndOffset(index, false);
    if (nullptr == bucket)
      return;

    auto &mb_item = bucket->items[offset];
    mb_item.reset();
  }

  std::optional<T> Take(size_t index) {
    auto [bucket, offset] = GetBucketAndOffset(index, false);
    if (nullptr == bucket)
      return std::nullopt;

    auto &mb_item = bucket->items[offset];
    std::optional<T> result = std::move(mb_item);

    mb_item.reset();
    return result;
  }

  T TakeExisting(size_t index) {
    auto [bucket, offset] = GetBucketAndOffset(index, false);
    assert(nullptr != bucket);

    auto &mb_item = bucket->items[offset];
    T result = std::move(*mb_item);

    mb_item.reset();
    return result;
  }

private:
  std::pair<Bucket *, size_t> GetBucketAndOffset(size_t index,
                                                 bool create_if_needed) {
    auto create_new = [this]() -> Bucket * {
      auto *space = m_allocator.allocate(1);
      if (nullptr == space)
        return nullptr;

      return new (space) Bucket();
    };

    if (nullptr == m_head) {
      if (!create_if_needed)
        return {nullptr, 0};

      m_head = create_new();
      if (nullptr == m_head)
        return {nullptr, 0};
    }

    Bucket *current = m_head;
    while (index >= t_num_items_in_bucket) {
      if (nullptr == current->next) {
        if (!create_if_needed)
          return {nullptr, 0};

        current->next = create_new();
        if (nullptr == current->next)
          return {nullptr, 0};
      }

      current = current->next;
      index -= t_num_items_in_bucket;
    }

    return {current, index};
  }

  Allocator m_allocator;
  Bucket *m_head = nullptr;
};

} // namespace edr

#endif
