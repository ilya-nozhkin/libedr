#ifndef LIBEDR_UTIL_ADT_BITSTREAM_HPP
#define LIBEDR_UTIL_ADT_BITSTREAM_HPP

#include <cassert>
#include <concepts>
#include <memory>

namespace edr {

template <class T> class BitStreamBase {
public:
  explicit BitStreamBase(T *data, size_t offset = 0)
      : m_storage(data + (offset / (8 * sizeof(T)))),
        m_offset(offset % (8 * sizeof(T))) {}

  template <std::unsigned_integral U> U Read(size_t num_bits) {
    assert(num_bits <= 8 * sizeof(U));

    U result = 0;
    size_t out_offset = 0;
    while (num_bits != 0) {
      auto left_ahead = 8 * sizeof(T) - m_offset;
      auto this_time = std::min<size_t>(left_ahead, num_bits);

      T in_piece = ((*m_storage) >> m_offset);

      if (this_time != 8 * sizeof(T)) {
        auto mask = static_cast<T>((1ULL << this_time) - 1);
        in_piece &= mask;
      }

      U out_piece = static_cast<U>(in_piece) << out_offset;

      result |= out_piece;

      out_offset += this_time;
      num_bits -= this_time;

      m_offset += this_time;
      if (m_offset == 8 * sizeof(T)) {
        m_offset = 0;
        m_storage += 1;
      }
    }

    return result;
  }

  size_t GetOffset() const { return m_offset; }

protected:
  T *m_storage;
  size_t m_offset;
};

template <class T> class BitStream;

template <std::unsigned_integral T>
class BitStream<const T> final : public BitStreamBase<const T> {
public:
  explicit BitStream(const T *data, size_t offset = 0)
      : BitStreamBase<const T>(data, offset) {}
};

template <std::unsigned_integral T>
class BitStream<T> final : public BitStreamBase<T> {
public:
  explicit BitStream(T *data, size_t offset = 0)
      : BitStreamBase<T>(data, offset) {}

  template <std::unsigned_integral U> void Write(U value, size_t num_bits) {
    assert(num_bits <= 8 * sizeof(U));

    while (num_bits != 0) {
      auto left_ahead = 8 * sizeof(T) - this->m_offset;
      auto this_time = std::min<size_t>(left_ahead, num_bits);

      T mask = this_time == 8 * sizeof(T)
                   ? ~static_cast<T>(0)
                   : static_cast<T>((1ULL << this_time) - 1);
      mask = mask << this->m_offset;

      T piece = static_cast<T>(value) << this->m_offset;

      (*this->m_storage) = (*this->m_storage) & ~mask | piece;

      value >>= this_time;
      num_bits -= this_time;

      this->m_offset += this_time;
      if (this->m_offset == 8 * sizeof(T)) {
        this->m_offset = 0;
        this->m_storage += 1;
      }
    }
  }

  template <class U> void Write(BitStream<U> &src, size_t num_bits) {
    using Vessel = std::conditional_t<sizeof(T) >= sizeof(U), T, U>;

    while (num_bits != 0) {
      size_t this_time = std::min<size_t>(8 * sizeof(Vessel), num_bits);
      auto vessel = src.template Read<Vessel>(this_time);
      Write(vessel, this_time);

      num_bits -= this_time;
    }
  }
};

template <class T> BitStream(T *data, size_t offset) -> BitStream<T>;
template <class T> BitStream(T *data) -> BitStream<T>;

} // namespace edr

#endif
