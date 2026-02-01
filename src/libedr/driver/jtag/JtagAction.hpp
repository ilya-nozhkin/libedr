#ifndef LIBEDR_DRIVER_JTAG_JTAGACTION_HPP
#define LIBEDR_DRIVER_JTAG_JTAGACTION_HPP

#include "libedr/driver/Action.hpp"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/util/adt/BitStream.hpp"
#include "libedr/util/miscellaneous/Formatting.hpp"
#include "libedr/util/vss/VSS.hpp"
#include "libedr/util/vss/VSSPayloads.hpp"

#include <cstddef>
#include <span>

namespace edr {

struct PutTMS {
  static inline constexpr auto g_id = ActionID::PutTMS;

  const uint32_t num_bits;
  using Payload = vss::Payload<vss::DependentBits<uint32_t>>;

  template <class T>
  PutTMS(const BitView<T> &bits) : num_bits(bits.GetNumBits()) {}

  BitView<uint32_t> Bits() {
    return BitView(vss::Get<0>(*this).Stream(), num_bits);
  }

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("TMS <-");
    fmt.Value("{}", Bits());
  }

  template <class T>
  static void EmplacePayload(vss::OutputStream auto &os,
                             const BitView<T> &bits) {
    Payload::Emplace(os, bits);
  }

  std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size) {
    return payload.Size(max_size, num_bits);
  }

  struct Out {
    uint32_t num_put;

    template <class T> Out(const BitView<T> &bits) {}

    template <StructureFormatter F> void Format(F &fmt) {
      fmt.Value("{}", num_put);
    }
  };

  PutTMS(const PutTMS &) = delete;
  PutTMS(PutTMS &&) = delete;
};

struct PutTDI {
  static inline constexpr auto g_id = ActionID::PutTDI;

  const uint32_t num_bits;
  const uint32_t last_tms : 1;
  using Payload = vss::Payload<vss::DependentBits<uint32_t>>;

  template <class T>
  PutTDI(const BitView<T> &bits, uint32_t last_tms)
      : num_bits(bits.GetNumBits()), last_tms(last_tms) {}

  BitView<uint32_t> Bits() {
    return BitView(vss::Get<0>(*this).Stream(), num_bits);
  }

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("TDI <-");
    fmt.Value("{}", Bits());
  }

  template <class T>
  static void EmplacePayload(vss::OutputStream auto &os, const BitView<T> &bits,
                             uint32_t last_tms) {
    Payload::Emplace(os, bits);
  }

  std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size) {
    return payload.Size(max_size, num_bits);
  }

  struct Out {
    uint32_t num_put;

    template <class T> Out(const BitView<T> &bits, uint32_t last_tms) {}

    template <StructureFormatter F> void Format(F &fmt) {
      fmt.Value("{}", num_put);
    }
  };

  PutTDI(const PutTDI &) = delete;
  PutTDI(PutTDI &&) = delete;
};

struct PutTDIGetTDO {
  static inline constexpr auto g_id = ActionID::PutTDIGetTDO;

  const uint32_t num_bits;
  const uint32_t last_tms : 1;
  using Payload = vss::Payload<vss::DependentBits<uint32_t>>;

  template <class T>
  PutTDIGetTDO(const BitView<T> &bits, uint32_t last_tms)
      : num_bits(bits.GetNumBits()), last_tms(last_tms) {}

  BitView<uint32_t> Bits() {
    return BitView(vss::Get<0>(*this).Stream(), num_bits);
  }

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("TDI-TDO <-");
    fmt.Value("{}", Bits());
  }

  template <class T>
  static void EmplacePayload(vss::OutputStream auto &os, const BitView<T> &bits,
                             uint32_t last_tms) {
    Payload::Emplace(os, bits);
  }

  std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size) {
    return payload.Size(max_size, num_bits);
  }

  struct Out {
    uint32_t num_put;
    using Payload = vss::Payload<vss::DependentBits<uint32_t>>;

    template <class T> Out(const BitView<T> &bits, uint32_t last_tms) {}

    BitView<uint32_t> Bits() {
      return BitView(vss::Get<0>(*this).Stream(), num_put);
    }

    template <StructureFormatter F> void Format(F &fmt) {
      fmt.Value("{}", Bits());
    }

    template <class T>
    static void EmplacePayload(vss::OutputStream auto &os,
                               const BitView<T> &bits, uint32_t last_tms) {
      Payload::Emplace(os, bits.GetNumBits());
    }

    std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size,
                                        PutTDIGetTDO &action) {
      return payload.Size(max_size, action.num_bits);
    }

    Out(const Out &) = delete;
    Out(Out &&) = delete;
  };

  PutTDIGetTDO(const PutTDI &) = delete;
  PutTDIGetTDO(PutTDI &&) = delete;
};

using JtagAction = Action<PutTMS>;

} // namespace edr

#endif
