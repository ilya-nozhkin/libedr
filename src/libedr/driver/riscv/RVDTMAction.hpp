#ifndef LIBEDR_DRIVER_RISCV_RVDTMACTION_HPP
#define LIBEDR_DRIVER_RISCV_RVDTMACTION_HPP

#include "libedr/driver/Action.hpp"
#include "libedr/driver/CommonActions.hpp"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/util/miscellaneous/Formatting.hpp"

namespace edr {

struct RVWriteDMI {
  static inline constexpr auto g_id = ActionID::RVWriteDMI;

  const uint32_t address;
  const uint32_t data;

  RVWriteDMI(uint32_t address, uint32_t data) : address(address), data(data) {}

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("DMI-W");
    fmt.Value("{:#x} <-", address);
    fmt.Value("{:#x}", data);
  }

  struct Out {
    Out(uint32_t address, uint32_t data) {}

    template <StructureFormatter F> void Format(F &fmt) { fmt.Value("OK"); }
  };
};

struct RVReadDMI {
  static inline constexpr auto g_id = ActionID::RVReadDMI;

  const uint32_t address;

  RVReadDMI(uint32_t address) : address(address) {}

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("DMI-R");
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

using RVDTMAction = Action<RVWriteDMI, RVReadDMI, SkipCycles>;

} // namespace edr

#endif
