#ifndef LIBEDR_UTIL_MISCELLANEOUS_FORMATTING_HPP
#define LIBEDR_UTIL_MISCELLANEOUS_FORMATTING_HPP

#include <cstdint>
#include <format>
#include <iterator>
#include <string_view>
#include <utility>

namespace edr {

template <class T>
concept StructureFormatter = requires(T refl) {
  refl.Name("Entity");

  refl.Value("{}", "A");
  refl.Value("{:x}", 10);

  refl.Field("f", "{}", "A");
  refl.Field("f", "{:x}", 10);
};

template <class TIterator> class PrintingStructureFormatter {
public:
  PrintingStructureFormatter(TIterator &out) : m_out(out) {}

  void Name(std::string_view name) {
    if (first)
      m_out = std::format_to(m_out, "{}", name);
    else
      m_out = std::format_to(m_out, " {}", name);

    first = false;
  }

  template <class... Args>
  void Value(std::format_string<Args...> format, Args &&...args) {
    if (!first)
      m_out = std::format_to(m_out, " ");

    first = false;

    m_out = std::format_to(m_out, format, std::forward<Args>(args)...);
  }

  template <class... Args>
  void Field(std::string_view name, std::format_string<Args...> format,
             Args &&...args) {
    if (first)
      m_out = std::format_to(m_out, "{}=", name);
    else
      m_out = std::format_to(m_out, " {}=", name);

    first = false;

    m_out = std::format_to(m_out, format, std::forward<Args>(args)...);
  }

private:
  TIterator &m_out;
  bool first = true;
};

template <class T>
concept StructurallyFormattable = requires() {
  &T::template Format<
      PrintingStructureFormatter<std::back_insert_iterator<std::string>>>;
};

template <class T> struct FormatAdapter {
  T &&obj;
};

template <class T> FormatAdapter<T> Format(T &&obj) {
  return {std::forward<T>(obj)};
}

} // namespace edr

template <edr::StructurallyFormattable T> struct std::formatter<T, char> {
  template <class ParseContext>
  constexpr ParseContext::iterator parse(ParseContext &ctx) {
    auto it = ctx.begin();
    while (*it != '}')
      it++;

    return it;
  }

  template <class FmtContext>
  FmtContext::iterator format(T &obj, FmtContext &ctx) const {
    auto it = ctx.out();
    edr::PrintingStructureFormatter<decltype(it)> formatter(it);
    obj.Format(formatter);
    return it;
  }
};

template <class T>
struct std::formatter<edr::FormatAdapter<std::span<T>>, char>
    : public std::formatter<T, char> {
  template <class FmtContext>
  FmtContext::iterator format(const edr::FormatAdapter<std::span<T>> &wrapped,
                              FmtContext &ctx) const {
    auto it = ctx.out();

    it = std::format_to(it, "{} [", wrapped.obj.size());
    for (size_t i = 0; i < wrapped.obj.size(); i++) {
      ctx.advance_to(it);
      it = std::formatter<T, char>::template format<FmtContext>(wrapped.obj[i],
                                                                ctx);

      if (i + 1 != wrapped.obj.size())
        it = std::format_to(it, " ");
    }

    it = std::format_to(it, "]");

    return it;
  }
};

template <>
struct std::formatter<edr::FormatAdapter<std::span<const std::byte>>, char> {
  template <class ParseContext>
  constexpr ParseContext::iterator parse(ParseContext &ctx) {
    auto it = ctx.begin();
    while (*it != '}')
      it++;

    return it;
  }

  template <class FmtContext>
  FmtContext::iterator
  format(const edr::FormatAdapter<std::span<const std::byte>> &wrapped,
         FmtContext &ctx) const {
    auto it = ctx.out();

    it = std::format_to(it, "{} [", wrapped.obj.size());
    for (size_t i = 0; i < wrapped.obj.size(); i++)
      if (i + 1 == wrapped.obj.size())
        it = std::format_to(it, "{:02x}", static_cast<uint8_t>(wrapped.obj[i]));
      else
        it =
            std::format_to(it, "{:02x} ", static_cast<uint8_t>(wrapped.obj[i]));

    it = std::format_to(it, "]");

    return it;
  }
};

template <>
struct std::formatter<edr::FormatAdapter<std::span<std::byte>>, char> {
  template <class ParseContext>
  constexpr ParseContext::iterator parse(ParseContext &ctx) {
    return std::formatter<edr::FormatAdapter<std::span<const std::byte>>,
                          char>{}
        .parse(ctx);
  }

  template <class FmtContext>
  FmtContext::iterator
  format(const edr::FormatAdapter<std::span<std::byte>> &wrapped,
         FmtContext &ctx) const {
    return std::formatter<edr::FormatAdapter<std::span<const std::byte>>,
                          char>{}
        .format(
            edr::Format(static_cast<std::span<const std::byte>>(wrapped.obj)),
            ctx);
  }
};

#endif
