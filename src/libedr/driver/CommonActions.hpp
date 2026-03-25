#ifndef LIBEDR_DRIVER_COMMONACTIONS_HPP
#define LIBEDR_DRIVER_COMMONACTIONS_HPP

#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/util/miscellaneous/Formatting.hpp"

namespace edr {

struct SkipCycles {
  static inline constexpr auto g_id = ActionID::SkipCycles;

  const uint32_t num_cycles;

  SkipCycles(uint32_t num_cycles) : num_cycles(num_cycles) {}

  template <StructureFormatter F> void Format(F &fmt) {
    fmt.Name("Skip-C");
    fmt.Value("{}", num_cycles);
  }

  struct Out {
    Out(uint32_t /*num_cycles*/) {}

    template <StructureFormatter F> void Format(F &fmt) { fmt.Value("OK"); }
  };
};

} // namespace edr

#endif
