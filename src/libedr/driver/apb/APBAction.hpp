#ifndef LIBEDR_DRIVER_APB_APBACTION_HPP
#define LIBEDR_DRIVER_APB_APBACTION_HPP

#include "libedr/driver/Action.hpp"
#include "libedr/driver/CommonActions.hpp"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/util/miscellaneous/Formatting.hpp"

namespace edr {

struct APBWrite {
  static inline constexpr auto g_id = ActionID::APBWrite;

  const uint32_t address;
  const uint32_t data;

  APBWrite(uint32_t address, uint32_t data) : address(address), data(data) {}

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("APB-W");
    fmt.Value("{:#x} <-", address);
    fmt.Value("{:#x}", data);
  }

  struct Out {
    Out(uint32_t address, uint32_t data) {}

    template <StructureFormatter F> void Format(F &fmt) { fmt.Value("OK"); }
  };
};

struct APBRead {
  static inline constexpr auto g_id = ActionID::APBRead;

  const uint32_t address;

  APBRead(uint32_t address) : address(address) {}

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("APB-R");
    fmt.Value("{:#x}", address);
  }

  struct Out {
    uint32_t data;

    Out(uint32_t address) {}

    template <StructureFormatter F> void Format(F &fmt) {
      fmt.Value("{:#x}", data);
    }
  };
};

struct APBSetPSEL {
  static inline constexpr auto g_id = ActionID::APBSetPSEL;

  const uint32_t psel;

  APBSetPSEL(uint32_t psel) : psel(psel) {}

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("APB-Set-PSEL");
    fmt.Value("{:#x}", psel);
  }

  struct Out {
    Out(uint32_t psel) {}

    template <StructureFormatter F> void Format(F &fmt) { fmt.Value("OK"); }
  };
};

using APBAction = Action<SkipCycles, APBWrite, APBRead, APBSetPSEL>;

} // namespace edr

#endif
