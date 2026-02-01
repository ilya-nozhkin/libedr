#ifndef LIBEDR_UTIL_ADT_BITSTREAM_HPP
#define LIBEDR_UTIL_ADT_BITSTREAM_HPP

#include <cassert>
#include <charconv>
#include <concepts>
#include <format>
#include <memory>
#include <string_view>

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

  void Skip(size_t num_bits) {
    m_offset += num_bits;

    auto num_advances = m_offset / (8 * sizeof(T));
    m_offset %= (8 * sizeof(T));
    m_storage += num_advances;
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

template <class T> class BitView {
public:
  explicit BitView(T *data, size_t num_bits, size_t offset = 0)
      : m_data(data, offset), m_num_bits(num_bits) {}

  explicit BitView(const BitStream<T> &data, size_t num_bits)
      : m_data(data), m_num_bits(num_bits) {}

  size_t GetNumBits() const { return m_num_bits; }

  BitStream<T> Stream(size_t offset = 0) const {
    auto copy = m_data;
    if (0 != offset)
      copy.Skip(offset);

    return copy;
  }

private:
  BitStream<T> m_data;
  size_t m_num_bits;
};

} // namespace edr

template <class T> struct std::formatter<edr::BitView<T>, char> {
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
  FmtContext::iterator format(const edr::BitView<T> &bits,
                              FmtContext &ctx) const {
    constexpr size_t num_chunk_bits = 8 * sizeof(T);

    auto it = ctx.out();

    size_t num_bits = bits.GetNumBits();

    it = std::format_to(it, "({} {}) ", num_bits,
                        reverse ? "reversed" : "forward");

    if (reverse) {
      while (num_bits != 0) {
        size_t this_time = std::min<size_t>(num_chunk_bits, num_bits);
        size_t offset = num_bits - this_time;

        edr::BitStream<T> stream = bits.Stream(offset);
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
      edr::BitStream<T> stream = bits.Stream();
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
