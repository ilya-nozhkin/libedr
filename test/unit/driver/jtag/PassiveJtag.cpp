#include "libedr/driver/jtag/JtagAction.hpp"
#include "test/unit/driver/Mocks.h"

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/jtag/PassiveJtag.h"

#include <gtest/gtest.h>

using namespace edr;

TEST(PassiveJtag, generates_tms_tdi) {
  MockLoggerOutput logger_output(LogLevel::ERROR);
  Logger logger(logger_output, LogLevel::TRACE);
  DriverContext ctx(logger);
  PassiveJtag jtag(ctx, "JTAG");

  auto tx = jtag.Initiate("tx");
  tx.Add<PutTMS>(BitStream(0x123u, 12));
  tx.Add<PutTMS>(BitStream(0xFFFu, 7));
  tx.Add<PutTDI>(BitStream(0x12345678FFFull, 41), 0);
  tx.Add<PutTDIGetTDO>(BitStream(0x54321u, 20), 1);

  jtag.Schedule(std::move(tx));

  uint32_t tms = 0;
  uint32_t tdi = 0;

  BitStream tms_dest(reinterpret_cast<uint8_t *>(&tms), 8 * sizeof(tms));
  BitStream tdi_dest(reinterpret_cast<uint8_t *>(&tdi), 8 * sizeof(tdi));

  jtag.PullTMSTDI(tms_dest, tdi_dest);

  EXPECT_EQ(tms, 0x0007F123u);
  EXPECT_EQ(tdi, 0x7FF80000u);

  uint64_t tms2 = 0;
  uint64_t tdi2 = 0;
  tms_dest = BitStream(reinterpret_cast<uint8_t *>(&tms2), 8 * sizeof(tms2));
  tdi_dest = BitStream(reinterpret_cast<uint8_t *>(&tdi2), 8 * sizeof(tdi2));

  jtag.PullTMSTDI(tms_dest, tdi_dest);

  EXPECT_EQ(tms2, 1ull << 47);
  EXPECT_EQ(tdi2, (0x54321ull << 28) | (0x12345678FFFull >> 13));
}
