#ifndef LIBEDR_UTIL_ADT_BITSTREAM_HPP
#define LIBEDR_UTIL_ADT_BITSTREAM_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <format>
#include <string_view>

namespace edr {

template <class T> class BitStream;

template <class T> class BitStreamBase {
  template <class U> friend class BitStream;

public:
  explicit BitStreamBase(T *data, size_t num_bits, size_t offset = 0)
      : m_storage(data + (offset / (8 * sizeof(T)))),
        m_offset(offset % (8 * sizeof(T))), m_remaining(num_bits - offset) {
    assert(offset <= num_bits);
  }

  template <std::unsigned_integral U> U Read(size_t num_bits) {
    assert(num_bits <= 8 * sizeof(U));

    num_bits = std::min(num_bits, m_remaining);

    return ReadUnsafe<U>(num_bits);
  }

  size_t Skip(size_t num_bits) {
    num_bits = std::min(num_bits, m_remaining);
    m_remaining -= num_bits;

    m_offset += num_bits;

    auto num_advances = m_offset / (8 * sizeof(T));
    m_offset %= (8 * sizeof(T));
    m_storage += num_advances;

    return num_bits;
  }

  void Crop(size_t num_bits) { m_remaining = std::min(m_remaining, num_bits); }

  std::pair<BitStream<T>, BitStream<T>> Split(size_t num_bits) {
    auto left = BitStream<T>(m_storage, m_remaining, m_offset);
    left.m_remaining = std::min(left.m_remaining, num_bits);

    auto right = BitStream<T>(m_storage, m_remaining, m_offset);
    right.Skip(num_bits);

    return {left, right};
  }

  size_t GetNumBits() const { return m_remaining; }

protected:
  template <std::unsigned_integral U> U ReadUnsafe(size_t num_bits) {
    m_remaining -= num_bits;

    std::remove_const_t<U> result = 0;
    size_t out_offset = 0;
    while (num_bits != 0) {
      auto left_ahead = 8 * sizeof(T) - m_offset;
      auto this_time = std::min<size_t>(left_ahead, num_bits);

      std::remove_const_t<T> in_piece = ((*m_storage) >> m_offset);

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

  T *m_storage;
  size_t m_offset;
  size_t m_remaining;
};

template <class T> class BitStream;

template <std::unsigned_integral T>
class BitStream<const T> final : public BitStreamBase<const T> {
public:
  BitStream(const BitStream<T> &stream)
      : BitStreamBase<const T>(stream.m_storage, stream.m_remaining,
                               stream.m_offset) {}

  BitStream(const T *data, size_t num_bits, size_t offset = 0)
      : BitStreamBase<const T>(data, num_bits, offset) {}

  BitStream(T data, size_t num_bits, size_t offset = 0)
      : BitStreamBase<const T>(&m_in_place, std::min(num_bits, 8 * sizeof(T)),
                               offset),
        m_in_place(data) {}

private:
  T m_in_place = 0;
};

template <std::unsigned_integral T>
class BitStream<T> final : public BitStreamBase<T> {
public:
  explicit BitStream(T *data, size_t num_bits, size_t offset = 0)
      : BitStreamBase<T>(data, num_bits, offset) {}

  template <std::unsigned_integral U> size_t Write(U value, size_t num_bits) {
    assert(num_bits <= 8 * sizeof(U));

    num_bits = std::min(num_bits, this->m_remaining);
    WriteUnsafe(value, num_bits);

    return num_bits;
  }

  template <class U> size_t Write(BitStream<U> &src, size_t num_bits) {
    num_bits = std::min(std::min(num_bits, this->m_remaining), src.m_remaining);
    size_t written = num_bits;

    using Vessel = std::conditional_t<sizeof(T) >= sizeof(U), T, U>;

    while (num_bits != 0) {
      size_t this_time = std::min<size_t>(8 * sizeof(Vessel), num_bits);
      auto vessel = src.template ReadUnsafe<Vessel>(this_time);
      WriteUnsafe(vessel, this_time);

      num_bits -= this_time;
    }

    return written;
  }

private:
  template <std::unsigned_integral U>
  void WriteUnsafe(U value, size_t num_bits) {
    this->m_remaining -= num_bits;

    while (num_bits != 0) {
      auto left_ahead = 8 * sizeof(T) - this->m_offset;
      auto this_time = std::min<size_t>(left_ahead, num_bits);

      T mask = this_time == 8 * sizeof(T)
                   ? ~static_cast<T>(0)
                   : static_cast<T>((1ULL << this_time) - 1);
      mask = mask << this->m_offset;

      T piece = static_cast<T>(value) << this->m_offset;

      (*this->m_storage) = (*this->m_storage) & ~mask | (piece & mask);

      value >>= this_time;
      num_bits -= this_time;

      this->m_offset += this_time;
      if (this->m_offset == 8 * sizeof(T)) {
        this->m_offset = 0;
        this->m_storage += 1;
      }
    }
  }
};

template <class T>
BitStream(T *data, size_t num_bits, size_t offset) -> BitStream<T>;
template <class T> BitStream(T *data, size_t num_bits) -> BitStream<T>;

template <class T> BitStream(T data, size_t num_bits) -> BitStream<const T>;
template <class T>
BitStream(T data, size_t num_bits, size_t offset) -> BitStream<const T>;

} // namespace edr

template <class T> struct std::formatter<edr::BitStream<T>, char> {
  bool reverse = false;

  template <class ParseContext>
  constexpr ParseContext::iterator parse(ParseContext &ctx) {
    auto it = ctx.begin();
    while (*it != '}') {
      if (*it == 'r')
        reverse = true;

      it++;
    }

    return it;
  }

  template <class FmtContext>
  FmtContext::iterator format(const edr::BitStream<T> &bits,
                              FmtContext &ctx) const {
    constexpr size_t num_chunk_bits = 8 * sizeof(T);

    auto it = ctx.out();

    edr::BitStream<T> stream = bits;
    size_t num_bits = stream.GetNumBits();

    it = std::format_to(it, "({} {}) ", num_bits,
                        reverse ? "lsb-right" : "lsb-left");

    if (reverse) {
      while (num_bits != 0) {
        size_t this_time = std::min<size_t>(num_chunk_bits, num_bits);
        size_t offset = num_bits - this_time;

        T chunk = stream.template Read<T>(this_time);

        char buffer[num_chunk_bits];
        for (size_t i = 0; i < this_time; i++)
          buffer[i] = 0 != (chunk & (static_cast<T>(1) << (this_time - i - 1)))
                          ? '1'
                          : '0';

        it = std::format_to(it, "{}", std::string_view(buffer, this_time));

        num_bits -= this_time;
      }
    } else {
      while (num_bits != 0) {
        size_t this_time = std::min<size_t>(num_chunk_bits, num_bits);
        T chunk = stream.template Read<T>(this_time);

        char buffer[num_chunk_bits];
        for (size_t i = 0; i < this_time; i++)
          buffer[i] = 0 != (chunk & (static_cast<T>(1) << i)) ? '1' : '0';

        it = std::format_to(it, "{}", std::string_view(buffer, this_time));

        num_bits -= this_time;
      }
    }

    return it;
  }
};

#endif
