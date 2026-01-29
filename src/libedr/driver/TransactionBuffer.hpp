#ifndef LIBEDR_DRIVER_TRANSACTIONBUFFER_HPP
#define LIBEDR_DRIVER_TRANSACTIONBUFFER_HPP

#include "libedr/util/vss/VSS.hpp"

#include <cstddef>
#include <memory_resource>
#include <span>

namespace edr {

template <size_t t_min_chunk_size> class TransactionBuffer {
public:
  static inline constexpr size_t g_alignment = 4;

  static_assert(0 == (t_min_chunk_size % alignof(std::max_align_t)));

  TransactionBuffer(std::pmr::memory_resource &res) : m_res(&res) {}

  TransactionBuffer(TransactionBuffer &&from)
      : m_res(from.m_res), m_head(from.m_head), m_tail(from.m_tail),
        m_offset(from.m_offset),
        m_had_allocation_failure(from.m_had_allocation_failure) {
    from.m_head = nullptr;
  }

  TransactionBuffer &operator=(TransactionBuffer &&from) {
    DeleteFrom(m_head);

    m_res = from.m_res;
    m_head = from.m_head;
    m_tail = from.m_tail;
    m_offset = from.m_offset;
    m_had_allocation_failure = from.m_had_allocation_failure;

    from.m_head = nullptr;
    return *this;
  }

  TransactionBuffer(const TransactionBuffer &) = delete;
  TransactionBuffer &operator=(const TransactionBuffer &) = delete;

  ~TransactionBuffer() {
    DeleteFrom(m_head);
    m_head = nullptr;
  }

  struct alignas(alignof(std::max_align_t)) Header {
    size_t total_size;
    size_t allocated;
    Header *next;
  };

  static inline constexpr size_t g_initial_offset = sizeof(Header);

  struct Position {
    Header *chunk;
    size_t offset;

    operator bool() { return nullptr != chunk; }

    bool operator==(const Position &to) const {
      return chunk == to.chunk && offset == to.offset;
    }
  };

  class InputStream {
  public:
    static inline constexpr size_t g_alignment = 4;

    InputStream(const Position &pos)
        : m_chunk(pos.chunk), m_offset(pos.offset) {}

    template <size_t t_new_alignment> void Align() {
      m_offset = (m_offset + (t_new_alignment - 1)) & ~(t_new_alignment - 1);
    }

    void *Get() { return reinterpret_cast<std::byte *>(m_chunk) + m_offset; }

    size_t NumContiguousAhead() { return m_chunk->allocated - m_offset; }

    void Advance(size_t size) {
      m_offset += size;
      if (m_offset >= m_chunk->allocated && nullptr != m_chunk->next) {
        m_chunk = m_chunk->next;
        m_offset = g_initial_offset;
      }
    }

    Position Tell() { return {m_chunk, m_offset}; }

  private:
    Header *m_chunk;
    size_t m_offset;
  };

  template <size_t t_new_alignment> void Align() {
    m_offset = (m_offset + (t_new_alignment - 1)) & ~(t_new_alignment - 1);
    if (nullptr != m_tail) {
      m_tail->allocated = m_offset;
      assert(m_tail->allocated <= m_tail->total_size);
    }
  }

  static_assert(vss::InputStream<InputStream>);

  void Clear() {
    m_tail = nullptr;
    m_offset = g_initial_offset;
  }

  std::pair<Position, void *> Allocate(size_t size) {
    size_t new_offset = m_offset + size;

    if (nullptr == m_tail || new_offset > m_tail->total_size) {
      if (!GoToNextChunk(size))
        return {Position{nullptr, g_initial_offset}, nullptr};

      new_offset = m_offset + size;
    }

    size_t offset = m_offset;
    m_offset = new_offset;
    m_tail->allocated = new_offset;

    void *ptr = reinterpret_cast<std::byte *>(m_tail) + offset;

    return {Position{m_tail, offset}, ptr};
  }

  InputStream From(const Position &pos) { return InputStream(pos); }

  Position Begin() const {
    return {nullptr == m_tail ? nullptr : m_head, g_initial_offset};
  }

  Position End() const { return {m_tail, m_offset}; }

  size_t Distance(const Position &pos) {
    size_t total = 0;
    Header *current = m_head;
    while (current != nullptr && current != pos.chunk) {
      total += current->allocated - g_initial_offset;
      current = current->next;
    }

    return total + pos.offset - g_initial_offset;
  }

  Position FromDistance(size_t distance) {
    if (nullptr == m_head)
      return {nullptr, g_initial_offset};

    Header *current = m_head;
    while (current != m_tail &&
           current->allocated - g_initial_offset <= distance) {
      distance -= current->allocated;
      current = current->next;
    }

    return {current, distance + g_initial_offset};
  }

  std::pmr::memory_resource &GetMemoryResource() { return *m_res; }

  bool HadAllocationFailure() const { return m_had_allocation_failure; }

  template <class F> void ForEachChunk(F &&func) {
    if (nullptr == m_tail)
      return;

    Header *current = m_head;
    do {
      std::span<std::byte> data(reinterpret_cast<std::byte *>(current) +
                                    g_initial_offset,
                                current->allocated - g_initial_offset);
      func(data);

      if (current == m_tail)
        return;

      current = current->next;
    } while (true);
  }

private:
  bool GoToNextChunk(size_t min_data_size) {
    size_t total_size =
        std::max(sizeof(Header) + min_data_size, t_min_chunk_size);

    auto make_chunk = [this, total_size]() -> Header * {
      auto *ptr = m_res->allocate(total_size);
      if (nullptr == ptr) {
        m_had_allocation_failure = true;
        return nullptr;
      }

      auto *chunk = new (ptr) Header{
          .total_size = total_size,
          .allocated = g_initial_offset,
          .next = nullptr,
      };
      return chunk;
    };

    if (nullptr == m_tail) {
      if (nullptr != m_head) {
        if (m_head->total_size >= total_size) {
          m_head->allocated = g_initial_offset;

          m_tail = m_head;
          m_offset = g_initial_offset;
          return true;
        }

        DeleteFrom(m_head);
        m_head = nullptr;
      }

      auto *new_chunk = make_chunk();
      if (nullptr == new_chunk)
        return false;

      m_head = m_tail = new_chunk;
      m_offset = g_initial_offset;
      return true;
    }

    Header *next = m_tail->next;
    if (nullptr != next) {
      if (next->total_size >= total_size) {
        next->allocated = g_initial_offset;

        m_tail = next;
        m_offset = g_initial_offset;
        return true;
      }

      DeleteFrom(next);
      m_tail->next = nullptr;
    }

    next = make_chunk();
    if (nullptr == next)
      return false;

    m_tail->next = next;
    m_tail = next;
    m_offset = g_initial_offset;
    return true;
  }

  void DeleteFrom(Header *chunk) {
    while (nullptr != chunk) {
      Header *to_delete = chunk;
      chunk = chunk->next;

      m_res->deallocate(to_delete, to_delete->total_size);
    }
  }

  std::pmr::memory_resource *m_res;

  Header *m_head = nullptr;
  Header *m_tail = nullptr;

  size_t m_offset = g_initial_offset;
  bool m_had_allocation_failure = false;
};

static_assert(vss::OutputStream<TransactionBuffer<1024>>);

} // namespace edr

#endif
