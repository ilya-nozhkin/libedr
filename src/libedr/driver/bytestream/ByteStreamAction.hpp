#ifndef LIBEDR_DRIVER_BYTESTREAM_BYTESTREAMACTION_HPP
#define LIBEDR_DRIVER_BYTESTREAM_BYTESTREAMACTION_HPP

#include "libedr/driver/Action.hpp"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/util/miscellaneous/Formatting.hpp"
#include "libedr/util/vss/VSS.hpp"
#include "libedr/util/vss/VSSPayloads.hpp"

#include <cstddef>
#include <span>

namespace edr {

struct WriteBytes {
  static inline constexpr auto g_id = ActionID::WriteBytes;

  const uint32_t size;
  using Payload = vss::Payload<vss::DependentBytes>;

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("WRITE");
    fmt.Value("{}", edr::Format(Span()));
  }

  std::span<const std::byte> Span() {
    return {vss::Get<0>(*this).Data(), size};
  }

  WriteBytes(const std::span<const std::byte> &data) : size(data.size()) {}

  static void EmplacePayload(vss::OutputStream auto &os,
                             const std::span<const std::byte> &data) {
    Payload::Emplace(os, data);
  }

  std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size) {
    return payload.Size(max_size, size);
  }

  struct Out {
    uint32_t num_written;

    Out(const std::span<const std::byte> &) {}

    template <StructureFormatter F> void Format(F &fmt) {
      fmt.Value("{}", num_written);
    }
  };

  WriteBytes(const WriteBytes &) = delete;
  WriteBytes(WriteBytes &&) = delete;
};

struct ReadBytes {
  static inline constexpr auto g_id = ActionID::ReadBytes;

  const uint32_t size;

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("READ");
    fmt.Value("{}", size);
  }

  ReadBytes(size_t size) : size(size) {}

  struct Out {
    uint32_t num_read;
    using Payload = vss::Payload<vss::DependentBytes>;

    template <StructureFormatter F> void Format(F &fmt) {
      fmt.Value("{}", edr::Format(Span()));
    }

    void *Data() { return vss::Get<0>(*this).Data(); }

    std::span<std::byte> Span() {
      return {vss::Get<0>(*this).Data(), num_read};
    }

    Out(size_t size) {}

    static void EmplacePayload(vss::OutputStream auto &os, size_t size) {
      Payload::Emplace(os, size);
    }

    std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size,
                                        ReadBytes &rb) {
      return payload.Size(max_size, rb.size);
    }

    Out(const Out &) = delete;
    Out(Out &&) = delete;
  };
};

using ByteStreamAction = Action<WriteBytes, ReadBytes>;

} // namespace edr

#endif
