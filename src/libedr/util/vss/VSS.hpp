#ifndef LIBEDR_UTIL_VSS_VSS_HPP
#define LIBEDR_UTIL_VSS_VSS_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

namespace edr {
namespace vss {

consteval bool IsPo2(size_t value) {
  return 0 != value && 0 == (value & (value - 1));
}

template <class T>
concept OutputStream = requires(T stream, size_t size) {
  { T::g_alignment } -> std::convertible_to<size_t>;

  stream.template Align<4>();
  stream.template Align<8>();

  { stream.Allocate(size).second } -> std::convertible_to<void *>;
};

template <class T>
concept InputStream = requires(T stream, size_t size) {
  { T::g_alignment } -> std::convertible_to<size_t>;

  stream.template Align<4>();
  stream.template Align<8>();

  { stream.Get() } -> std::convertible_to<void *>;
  { stream.NumContiguousAhead() } -> std::convertible_to<size_t>;
  stream.Advance(size);
};

template <size_t t_alignment> class CountingStream {
public:
  static inline constexpr size_t g_alignment = t_alignment;

  template <size_t t_new_alignment> void Align() {
    static_assert(IsPo2(t_new_alignment));
    m_offset = (m_offset + (t_new_alignment - 1)) & ~(t_new_alignment - 1);
  }

  std::pair<size_t, void *> Allocate(size_t size) {
    auto offset = m_offset;
    m_offset += size;
    return {offset, nullptr};
  }

  size_t Size() { return m_offset; }

private:
  size_t m_offset = 0;
};

static_assert(OutputStream<CountingStream<4>>);

template <size_t t_alignment> class ContiguousOutputStream {
public:
  static inline constexpr size_t g_alignment = t_alignment;

  ContiguousOutputStream(void *start)
      : m_start(reinterpret_cast<std::byte *>(start)) {}

  template <size_t t_new_alignment> void Align() {
    static_assert(IsPo2(t_new_alignment));
    m_offset = (m_offset + (t_new_alignment - 1)) & ~(t_new_alignment - 1);
  }

  size_t Tell() { return m_offset; }

  std::pair<size_t, void *> Allocate(size_t size) {
    auto offset = m_offset;
    auto *ptr = m_start + offset;
    m_offset += size;
    return {offset, ptr};
  }

private:
  std::byte *m_start;
  size_t m_offset = 0;
};

static_assert(OutputStream<ContiguousOutputStream<4>>);

template <size_t t_alignment> class ContiguousInputStream {
public:
  static inline constexpr size_t g_alignment = t_alignment;

  ContiguousInputStream(void *start, size_t size)
      : m_start(reinterpret_cast<std::byte *>(start)), m_size(size) {}

  template <size_t t_new_alignment> void Align() {
    static_assert(IsPo2(t_new_alignment));
    m_offset = (m_offset + (t_new_alignment - 1)) & ~(t_new_alignment - 1);
  }

  void *Get() { return m_start + m_offset; }

  size_t NumContiguousAhead() { return m_size - m_offset; }

  void Advance(size_t size) { m_offset += size; }

  size_t GetOffset() { return m_offset; }

private:
  std::byte *m_start;
  size_t m_size;
  size_t m_offset = 0;
};

static_assert(InputStream<ContiguousInputStream<4>>);

template <OutputStream OS, size_t t_overriden_alignment>
class AligningOutputStream final {
public:
  static inline constexpr size_t g_alignment = t_overriden_alignment;

  AligningOutputStream(OS &wrapped) : m_wrapped(wrapped) {}

  template <size_t t_alignment> void Align() {
    m_wrapped.template Align<t_alignment>();
  }

  template <size_t size> auto Allocate() {
    static_assert(IsPo2(t_overriden_alignment));

    constexpr size_t mask = t_overriden_alignment - 1;
    constexpr size_t aligned_size = (size + mask) & ~mask;
    return m_wrapped.Allocate(aligned_size);
  }

  auto Allocate(size_t size) {
    static_assert(IsPo2(t_overriden_alignment));
    constexpr size_t mask = t_overriden_alignment - 1;

    size_t aligned_size = (size + mask) & ~mask;
    return m_wrapped.Allocate(aligned_size);
  }

  OS &GetWrapped() { return m_wrapped; }

private:
  OS &m_wrapped;
};

static_assert(OutputStream<AligningOutputStream<CountingStream<4>, 8>>);

template <InputStream IS, size_t t_overriden_alignment>
class AligningInputStream final {
public:
  static inline constexpr size_t g_alignment = t_overriden_alignment;

  AligningInputStream(IS &wrapped) : m_wrapped(wrapped) {}

  template <size_t t_alignment> void Align() {
    m_wrapped.template Align<t_alignment>();
  }

  void *Get() { return m_wrapped.Get(); }

  size_t NumContiguousAhead() { return m_wrapped.NumContiguousAhead(); }

  template <size_t size> void Advance() {
    static_assert(IsPo2(t_overriden_alignment));

    constexpr size_t mask = t_overriden_alignment - 1;
    constexpr size_t aligned_size = (size + mask) & ~mask;
    return m_wrapped.Advance(aligned_size);
  }

  void Advance(size_t size) {
    static_assert(IsPo2(t_overriden_alignment));
    constexpr size_t mask = t_overriden_alignment - 1;

    size_t aligned_size = (size + mask) & ~mask;
    return m_wrapped.Advance(aligned_size);
  }

  IS &GetWrapped() { return m_wrapped; }

private:
  IS &m_wrapped;
};

static_assert(InputStream<AligningInputStream<ContiguousInputStream<4>, 8>>);

template <class T>
concept HasPayload = requires() { typename T::Payload; };

template <class T>
concept HasCustomAlignment = requires() {
  { T::Alignment() } -> std::convertible_to<size_t>;
};

namespace {
template <class T> static consteval size_t GetNumParts() {
  if constexpr (HasPayload<T>)
    return T::Payload::g_num_parts;
  else
    return 0;
}

template <class T> static consteval size_t ComputeMaxAlignmentImpl() {
  if constexpr (HasPayload<T>) {
    size_t alignment = alignof(T);
    T::Payload::ForParts([&alignment]<class P, size_t I>() {
      alignment = std::max(alignment, ComputeMaxAlignmentImpl<P>());
    });

    return alignment;
  } else if constexpr (HasCustomAlignment<T>) {
    static_assert(T::Alignment() >= alignof(T));
    return T::Alignment();
  } else
    return alignof(T);
}

} // namespace

template <class T> static consteval size_t ComputeMaxAlignment() {
  constexpr size_t alignment = ComputeMaxAlignmentImpl<T>();

  static_assert(0 != alignment && 0 == (alignment & (alignment - 1)));

  return alignment;
}

namespace {

template <class P, size_t I, size_t default_alignment>
static consteval size_t GetPartAlignment() {
  if constexpr (I >= P::g_num_parts)
    return default_alignment;
  else
    return P::template ForPart<I>(
        []<class Part>() { return ComputeMaxAlignment<Part>(); });
}

template <class T, size_t default_alignment>
static consteval size_t GetPayloadAlignment() {
  if constexpr (!HasPayload<T>)
    return default_alignment;
  else if constexpr (0 == GetNumParts<T>())
    return 1;
  else
    return T::Payload::template ForPart<0>(
        []<class Part>() { return ComputeMaxAlignment<Part>(); });
}

template <class T, size_t post_alignment, OutputStream OS, class... Args>
void EmplaceImpl(OS &os, Args &&...args) {
  static_assert(std::is_trivially_copyable_v<T>);
  static_assert(std::is_trivially_destructible_v<T>);

  if constexpr (HasPayload<T>) {
    static_assert(!std::is_copy_constructible_v<T>);
    static_assert(!std::is_copy_assignable_v<T>);
    static_assert(!std::is_move_constructible_v<T>);
    static_assert(!std::is_move_assignable_v<T>);
  }

  if constexpr (!std::is_empty_v<T>) {
    constexpr size_t next_alignment = GetPayloadAlignment<T, post_alignment>();
    AligningOutputStream<OS, next_alignment> self_os(os);

    auto [_, ptr] = self_os.template Allocate<sizeof(T)>();
    if (nullptr != ptr)
      new (ptr) T(std::forward<Args>(args)...);
  }

  if constexpr (HasPayload<T>) {
    AligningOutputStream<OS, post_alignment> payload_os(os);
    T::EmplacePayload(payload_os, std::forward<Args>(args)...);
  }
}

template <class Arg> struct ExpandArg {
  template <class F> static auto Do(F &&func, Arg &&arg) {
    return std::forward<F>(func)(std::forward<Arg>(arg));
  }
};

template <class... Args> struct ExpandArg<std::tuple<Args...>> {
  template <class F, class T> static auto Do(F &&func, T &&arg) {
    return std::apply(std::forward<F>(func), std::forward<T>(arg));
  }
};
} // namespace

template <size_t post_alignment, class T, class... Context>
std::optional<size_t> SizeOf(T *obj, size_t max_size, Context &&...ctx) {
  size_t total = 0;

  if constexpr (!std::is_empty_v<T>) {
    constexpr size_t next_alignment = GetPayloadAlignment<T, post_alignment>();
    constexpr size_t mask = next_alignment - 1;

    size_t aligned_size = (sizeof(T) + mask) & ~mask;
    if (aligned_size > max_size)
      return std::nullopt;

    total += aligned_size;
    max_size -= aligned_size;
  }

  if constexpr (HasPayload<T>) {
    auto *payload_byte_ptr = reinterpret_cast<std::byte *>(obj) + total;
    auto &payload = *reinterpret_cast<T::Payload *>(payload_byte_ptr);

    std::optional<size_t> mb_payload_size =
        obj->SizeOfPayload(payload, max_size, std::forward<Context>(ctx)...);
    if (!mb_payload_size)
      return std::nullopt;

    constexpr size_t mask = post_alignment - 1;
    size_t aligned_size = (*mb_payload_size + mask) & ~mask;

    if (aligned_size > max_size)
      return std::nullopt;

    total += aligned_size;
  }

  return total;
}

template <class T, class... Context>
size_t SizeOfTrusted(T &obj, Context &&...ctx) {
  auto mb_size = SizeOf<1>(&obj, std::numeric_limits<size_t>::max(),
                           std::forward<Context>(ctx)...);
  assert(mb_size);
  return *mb_size;
}

template <class... Parts> struct Payload {
public:
  static inline constexpr size_t g_num_parts = sizeof...(Parts);

  template <OutputStream OS, class... Args>
  static void Emplace(OS &os, Args &&...args) {
    if constexpr (1 == sizeof...(Parts))
      EmplaceImpl<Parts..., OS::g_alignment>(os.GetWrapped(),
                                             std::forward<Args>(args)...);
    else
      ForParts(
          [&os]<class P, size_t I, class Arg>(Arg &&arg) {
            ExpandArg<Arg>::Do(
                [&os]<class... EArgs>(EArgs &&...eargs) {
                  constexpr size_t next_alignment =
                      GetPartAlignment<Payload, I + 1, OS::g_alignment>();

                  EmplaceImpl<P, next_alignment>(os.GetWrapped(),
                                                 std::forward<EArgs>(eargs)...);
                },
                std::forward<Arg>(arg));
          },
          std::forward<Args>(args)...);
  }

  template <class... Context>
  std::optional<size_t> Size(size_t max_size, Context &&...ctx) {
    if constexpr (1 == sizeof...(Parts))
      return ForPart<0>([&]<class P>() {
        auto *part = reinterpret_cast<P *>(this);
        return vss::SizeOf<1>(part, max_size, std::forward<Context>(ctx)...);
      });
    else {
      size_t total = 0;
      bool failed = false;

      if constexpr (0 == sizeof...(ctx))
        ForParts([&]<class P, size_t I>() {
          if (failed)
            return;

          constexpr size_t next_alignment =
              GetPartAlignment<Payload, I + 1, 1>();

          P *part = reinterpret_cast<P *>(reinterpret_cast<std::byte *>(this) +
                                          total);

          std::optional<size_t> mb_size =
              vss::SizeOf<next_alignment>(part, max_size);
          if (!mb_size) {
            failed = true;
            return;
          }

          total += *mb_size;
          max_size -= *mb_size;
        });
      else
        ForParts(
            [&]<class P, size_t I, class Arg>(Arg &&arg) {
              ExpandArg<Arg>::Do(
                  [&]<class... EArgs>(EArgs &&...eargs) {
                    if (failed)
                      return;

                    constexpr size_t next_alignment =
                        GetPartAlignment<Payload, I + 1, 1>();

                    P *part = reinterpret_cast<P *>(
                        reinterpret_cast<std::byte *>(this) + total);

                    std::optional<size_t> mb_size = vss::SizeOf<next_alignment>(
                        part, max_size, std::forward<EArgs>(eargs)...);
                    if (!mb_size) {
                      failed = true;
                      return;
                    }

                    total += *mb_size;
                    max_size -= *mb_size;
                  },
                  std::forward<Arg>(arg));
            },
            std::forward<Context>(ctx)...);

      return failed ? std::nullopt : std::optional<size_t>(total);
    }
  }

  template <size_t N = sizeof...(Parts), class F, class... Args>
  static constexpr void ForParts(F &&func, Args &&...args) {
    static_assert(N == sizeof...(args));
    ForPartsImpl(std::forward<F>(func), std::make_index_sequence<N>(),
                 std::forward<Args>(args)...);
  }

  template <size_t N = sizeof...(Parts), class F>
  static constexpr void ForParts(F &&func) {
    ForPartsImpl(std::forward<F>(func), std::make_index_sequence<N>());
  }

  template <size_t I, class F> static constexpr auto ForPart(F &&func) {
    return func
        .template operator()<std::tuple_element_t<I, std::tuple<Parts...>>>();
  }

private:
  template <class F, size_t... I, class... Args>
  static constexpr void
  ForPartsImpl(F &&func, std::index_sequence<I...> indices, Args &&...args) {
    using HelperTuple = std::tuple<Parts...>;
    (
        [&]() {
          func.template operator()<std::tuple_element_t<I, HelperTuple>, I>(
              std::forward<Args>(args));
        }(),
        ...);
  }

  template <class F, size_t... I>
  static constexpr void ForPartsImpl(F &&func,
                                     std::index_sequence<I...> indices) {
    using HelperTuple = std::tuple<Parts...>;
    (
        [&]() {
          func.template operator()<std::tuple_element_t<I, HelperTuple>, I>();
        }(),
        ...);
  }
};

template <class T, OutputStream OS, class... Args>
auto Emplace(OS &os, Args &&...args) {
  constexpr size_t required_total_alignment = ComputeMaxAlignment<T>();
  if constexpr (required_total_alignment > OS::g_alignment)
    os.template Align<required_total_alignment>();

  if constexpr (!HasPayload<T>) {
    if constexpr (std::is_empty_v<T>) {
      auto [pos, _] = os.Allocate(0);
      return pos;
    } else {
      AligningOutputStream<OS, OS::g_alignment> self_os(os);

      auto [pos, ptr] = self_os.template Allocate<sizeof(T)>();
      if (nullptr != ptr)
        new (ptr) T(std::forward<Args>(args)...);

      return pos;
    }
  } else {
    CountingStream<OS::g_alignment> counting_os;
    EmplaceImpl<T, OS::g_alignment>(counting_os, std::forward<Args>(args)...);

    size_t total_size = counting_os.Size();
    auto [pos, ptr] = os.Allocate(total_size);
    if (nullptr != ptr) {
      ContiguousOutputStream<OS::g_alignment> contiguous_os(ptr);
      EmplaceImpl<T, OS::g_alignment>(contiguous_os,
                                      std::forward<Args>(args)...);
    }

    return pos;
  }
}

template <class T, InputStream IS, class... Context>
T *Extract(IS &is, Context &&...ctx) {
  constexpr size_t required_total_alignment = ComputeMaxAlignment<T>();
  if constexpr (required_total_alignment > IS::g_alignment)
    is.template Align<required_total_alignment>();

  auto *obj = reinterpret_cast<T *>(is.Get());

  std::optional<size_t> mb_size = SizeOf<IS::g_alignment>(
      obj, is.NumContiguousAhead(), std::forward<Context>(ctx)...);
  if (!mb_size)
    return nullptr;

  is.Advance(*mb_size);

  return obj;
}

template <size_t IHead, class T, class... Context>
auto &Get(T &obj, Context &&...ctx) {
  static_assert(IHead < GetNumParts<T>());

  size_t total = 0;

  if constexpr (!std::is_empty_v<T>) {
    constexpr size_t next_alignment = GetPayloadAlignment<T, 1>();
    constexpr size_t mask = next_alignment - 1;
    total += (sizeof(T) + mask) & ~mask;
  }

  if constexpr (0 != IHead) {
    if constexpr (0 == sizeof...(ctx))
      T::Payload::template ForParts<IHead>([&]<class P, size_t I>() {
        constexpr size_t next_alignment =
            GetPartAlignment<typename T::Payload, I + 1, 1>();

        auto *part =
            reinterpret_cast<P *>(reinterpret_cast<std::byte *>(&obj) + total);
        std::optional<size_t> mb_size = vss::SizeOf<next_alignment>(
            part, std::numeric_limits<size_t>::max());

        assert(mb_size);
        total += *mb_size;
      });
    else
      T::Payload::template ForParts<IHead>(
          [&]<class P, size_t I, class Arg>(Arg &&arg) {
            ExpandArg<Arg>::Do(
                [&]<class... EArgs>(EArgs &&...eargs) {
                  constexpr size_t next_alignment =
                      GetPartAlignment<typename T::Payload, I + 1, 1>();

                  auto *part = reinterpret_cast<P *>(
                      reinterpret_cast<std::byte *>(&obj) + total);
                  std::optional<size_t> mb_size = vss::SizeOf<next_alignment>(
                      part, std::numeric_limits<size_t>::max(),
                      std::forward<EArgs>(eargs)...);

                  assert(mb_size);
                  total += *mb_size;
                },
                std::forward<Arg>(arg));
          },
          std::forward<Context>(ctx)...);
  }

  auto *part_byte_ptr = reinterpret_cast<std::byte *>(&obj) + total;

  return T::Payload::template ForPart<IHead>([&]<class P>() {
           return std::ref(*reinterpret_cast<P *>(part_byte_ptr));
         })
      .get();
}

template <class T> struct CopyAdapter {
  T &ref;
};

template <class T> CopyAdapter<T> Copy(T &from) { return CopyAdapter<T>{from}; }

} // namespace vss
} // namespace edr

#endif
