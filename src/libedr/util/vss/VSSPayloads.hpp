#ifndef LIBEDR_UTIL_VSS_VSSPAYLOADS_HPP
#define LIBEDR_UTIL_VSS_VSSPAYLOADS_HPP

#include "libedr/util/adt/BitStream.hpp"
#include "libedr/util/vss/VSS.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace edr {
namespace vss {

template <class T> struct alignas(alignof(T)) DependentArray {
  template <class... Args> DependentArray(Args &&...) {}

  T *Data() { return reinterpret_cast<T *>(this); }

  const T *Data() const { return reinterpret_cast<const T *>(this); }

  using Payload = vss::Payload<>;

  static void EmplacePayload(vss::OutputStream auto &os,
                             size_t num_reserved_items) {
    os.Allocate(sizeof(T) * num_reserved_items);
  }

  static void EmplacePayload(vss::OutputStream auto &os,
                             const std::span<const T> &data) {
    size_t total_size = sizeof(T) * data.size();
    auto [_, ptr] = os.Allocate(total_size);
    if (nullptr == ptr)
      return;

    memcpy(ptr, data.data(), total_size);
  }

  std::optional<size_t> SizeOfPayload(Payload & /*payload*/,
                                      size_t /*max_size*/, size_t num_items) {
    return sizeof(T) * num_items;
  }

  DependentArray(const DependentArray &) = delete;
  DependentArray(DependentArray &&) = delete;
};

using DependentBytes = DependentArray<std::byte>;

static_assert(HasPayload<DependentBytes>);

struct String {
  const uint32_t length;

  using Payload = vss::Payload<DependentArray<char>>;

  String(std::string_view content) : length(content.size()) {}

  static void EmplacePayload(vss::OutputStream auto &os,
                             std::string_view content) {
    Payload::Emplace(os, std::span<const char>(content));
  }

  std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size) {
    return payload.Size(max_size, length);
  }

  std::string_view View() {
    return std::string_view(vss::Get<0>(*this).Data(), length);
  }

  struct Format {
    const uint32_t length;

    using Payload = vss::Payload</* DependentArray<char> */>;

    template <class... Args>
    Format(std::format_string<Args...> format, Args &&...args)
        : length(std::formatted_size(format, std::forward<Args>(args)...)) {}

    template <class... Args>
    static void EmplacePayload(vss::OutputStream auto &os,
                               std::format_string<Args...> format,
                               Args &&...args) {
      size_t size = std::formatted_size(format, std::forward<Args>(args)...);
      auto [_, ptr] = os.Allocate(size);
      if (nullptr == ptr)
        return;

      std::format_to_n(reinterpret_cast<char *>(ptr), size, format,
                       std::forward<Args>(args)...);
    }

    std::optional<size_t> SizeOfPayload(Payload & /*payload*/,
                                        size_t /*max_size*/) = delete;

    Format(const Format &) = delete;
    Format(Format &&) = delete;
  };

  String(const String &) = delete;
  String(String &&) = delete;
};

static_assert(HasPayload<String> && HasPayload<String::Format>);

template <class T> struct alignas(alignof(T)) DependentBits {
  using Payload = vss::Payload<>;

  BitStream<T> Stream(size_t num_bits, size_t offset = 0) {
    return BitStream<T>(reinterpret_cast<T *>(this), num_bits, offset);
  }

  static void EmplacePayload(vss::OutputStream auto &os,
                             size_t num_reserved_bits) {
    os.Allocate(GetNumBytes(num_reserved_bits));
  }

  template <class U>
  static void EmplacePayload(vss::OutputStream auto &os, BitStream<U> src) {
    auto num_bits = src.GetNumBits();

    auto [_, ptr] = os.Allocate(GetNumBytes(num_bits));
    if (nullptr == ptr)
      return;

    BitStream<T> writer(reinterpret_cast<T *>(ptr), num_bits);
    writer.Write(src, num_bits);
  }

  std::optional<size_t> SizeOfPayload(Payload & /*payload*/,
                                      size_t /*max_size*/, size_t num_bits) {
    return GetNumBytes(num_bits);
  }

  DependentBits(const DependentBits &) = delete;
  DependentBits(DependentBits &&) = delete;

private:
  static size_t GetNumBytes(size_t num_bits) {
    constexpr auto mask = 8 * sizeof(T) - 1;
    auto aligned_num_bits = (num_bits + mask) & ~mask;
    return aligned_num_bits / 8;
  }
};

namespace {

template <class Discriminant, class OHead, class... OTail>
struct SequentialDiscriminantGetter {
  template <class O> consteval Discriminant operator()() { return Impl<O>({}); }

  template <class O> static consteval Discriminant Impl(Discriminant counter) {
    if constexpr (std::is_same_v<O, OHead>)
      return counter;
    else
      return SequentialDiscriminantGetter<
          Discriminant, OTail...>::template Impl<O>(++counter);
  }
};

template <class DiscriminantGetter, class... Options, class Discriminant,
          class F>
bool ForOption(Discriminant discriminant, F &&func) {
  return ([&]() {
    if (DiscriminantGetter{}.template operator()<Options>() == discriminant) {
      func.template operator()<Options>();
      return true;
    }

    return false;
  }() || ...);
}

template <class... Options> consteval size_t ComputeMaxOptionAlignment() {
  size_t alignment = 1;
  (
      [&]() {
        alignment = std::max(alignment, vss::ComputeMaxAlignment<Options>());
      }(),
      ...);
  return alignment;
}

}; // namespace

template <class Discriminant, class DiscriminantGetter, class... Options>
struct VariantBase {
  const Discriminant discriminant;

  using Payload = vss::Payload<>;

  static consteval size_t Alignment() {
    return std::max(alignof(VariantBase),
                    ComputeMaxOptionAlignment<Options...>());
  }

  template <class T> static consteval bool IsValidOption() {
    return (std::is_same_v<T, Options> || ...);
  }

  bool IsValid() const {
    return (
        (discriminant == DiscriminantGetter{}.template operator()<Options>()) ||
        ...);
  }

  operator bool() const { return IsValid(); }

public:
  using AnyOption = std::tuple_element_t<0, std::tuple<Options...>>;

  template <class F>
  using VisitorReturnType = std::invoke_result_t<F, AnyOption &>;

  template <class T> T *As() {
    if (DiscriminantGetter{}.template operator()<T>() != discriminant)
      return nullptr;

    constexpr auto required_alignment = vss::ComputeMaxAlignment<T>();
    constexpr auto offset = std::max(required_alignment, sizeof(VariantBase));

    auto *ptr = reinterpret_cast<std::byte *>(this) + offset;
    return reinterpret_cast<T *>(ptr);
  }

  template <class F, class R = VisitorReturnType<F>>
  std::conditional_t<std::is_same_v<R, void>, bool, std::optional<R>>
  Visit(F &&func) {
    if constexpr (std::is_same_v<R, void>)
      return ForOption<DiscriminantGetter, Options...>(
          discriminant, [&]<class O>() { func(*As<O>()); });
    else {
      std::optional<R> result;
      ForOption<DiscriminantGetter, Options...>(
          discriminant, [&]<class O>() { result.emplace(func(*As<O>())); });
      return result;
    }
  }

  template <class... SubOptions>
  VariantBase(const CopyAdapter<VariantBase<Discriminant, DiscriminantGetter,
                                            SubOptions...>> &from)
      : discriminant(from.ref.discriminant) {}

  template <class O> struct Option {
    static_assert(IsValidOption<O>());

    const Discriminant discriminant;

    template <class... Args>
    Option(Args &&...)
        : discriminant(DiscriminantGetter{}.template operator()<O>()) {}

    using Payload = vss::Payload<O>;

    static consteval size_t Alignment() {
      return std::max(alignof(VariantBase),
                      ComputeMaxOptionAlignment<Options...>());
    }

    template <class... Args>
    static void EmplacePayload(vss::OutputStream auto &os, Args &&...args) {
      Payload::Emplace(os, std::forward<Args>(args)...);
    }

    std::optional<size_t> SizeOfPayload(Payload & /*payload*/,
                                        size_t /*max_size*/) = delete;

    Option(const Option &) = delete;
    Option(Option &&) = delete;
  };

  static void EmplacePayload(vss::OutputStream auto &os) = delete;

  template <class... SubOptions>
  static void EmplacePayload(
      vss::OutputStream auto &os,
      const CopyAdapter<
          VariantBase<Discriminant, DiscriminantGetter, SubOptions...>> &from) {
    static_assert((IsValidOption<SubOptions>() && ...));
    assert(from.ref.IsValid());

    from.ref.Visit([&]<class O>(O &from_payload) {
      void *source = &from_payload;
      size_t size = vss::SizeOfTrusted(from_payload);

      constexpr size_t required_alignment = vss::ComputeMaxAlignment<O>();
      constexpr size_t misalignment =
          required_alignment <= sizeof(VariantBase)
              ? 0
              : required_alignment - sizeof(VariantBase);

      auto [_, allocated_ptr] = os.Allocate(misalignment + size);
      if (nullptr == allocated_ptr)
        return;

      auto *dest = reinterpret_cast<std::byte *>(allocated_ptr) + misalignment;
      memcpy(dest, source, size);
    });
  }

  template <class... Args>
  std::optional<size_t> SizeOfPayload(Payload & /*payload*/, size_t max_size,
                                      Args &&...args) {
    std::optional<size_t> size = 0;
    Visit([&]<class O>(O &payload) {
      constexpr size_t required_alignment = vss::ComputeMaxAlignment<O>();
      constexpr size_t misalignment =
          required_alignment <= sizeof(VariantBase)
              ? 0
              : required_alignment - sizeof(VariantBase);

      if (misalignment > max_size) {
        size.reset();
        return;
      }

      std::optional<size_t> mb_payload_size =
          vss::SizeOf<1>(&payload, max_size - misalignment);
      if (!mb_payload_size) {
        size.reset();
        return;
      }

      size = misalignment + *mb_payload_size;
    });

    return size;
  }

  VariantBase(const VariantBase &) = delete;
  VariantBase(VariantBase &&) = delete;
};

template <class... Options>
using Variant =
    VariantBase<uint32_t, SequentialDiscriminantGetter<uint32_t, Options...>,
                Options...>;

static_assert(HasCustomAlignment<Variant<int, double>>);
static_assert(HasCustomAlignment<Variant<int, double>::Option<int>>);

} // namespace vss
} // namespace edr

#endif
