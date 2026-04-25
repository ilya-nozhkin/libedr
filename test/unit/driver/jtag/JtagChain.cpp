#include "libedr/driver/jtag/JtagChainAction.hpp"
#include "test/unit/driver/Mocks.h"

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/jtag/JtagChain.h"
#include "libedr/driver/jtag/PullJtag.h"

#include <cstdint>
#include <gtest/gtest.h>

using namespace edr;

TEST(JtagChain, can_access_first_tap_in_chain) {
  MockLoggerOutput logger_output(LogLevel::ERROR);
  Logger logger(logger_output, LogLevel::TRACE);
  DriverContext ctx(logger);
  PullJtag jtag(ctx, "JTAG");
  JtagChain chain(ctx, "JtagChain", jtag);

  auto tx = chain.Initiate("tx");
  tx.Add<JCSetIRLength>(0, 5);
  tx.Add<JCSetIRLength>(1, 4);
  tx.Add<JCSetIRLength>(2, 6);

  // 0th is selected, the highest 2 should be dropped
  tx.Add<JCWriteIR>(0b1010101);
  auto status = chain.Schedule(std::move(tx));

  uint64_t tms = 0;
  uint64_t tdi = 0;

  BitStream tms_dest(reinterpret_cast<uint8_t *>(&tms), 8 * sizeof(tms));
  BitStream tdi_dest(reinterpret_cast<uint8_t *>(&tdi), 8 * sizeof(tdi));

  size_t pulled = jtag.PullTMSTDI(tms_dest, tdi_dest);

  EXPECT_EQ(pulled, 25);
  EXPECT_EQ(tms, (1 << (14 + 10)) | 0b0011011111);
  EXPECT_EQ(tdi, (0b101011111111111) << 10);

  uint64_t tdo = 0;
  BitStream tdo_src(reinterpret_cast<const uint8_t *>(&tms), pulled);

  jtag.PushTDO(tdo_src);

  ASSERT_TRUE(status.done());
  ASSERT_TRUE(*status);

  tx = chain.Initiate("tx", &*status);
  tx.Add<JCShiftDR>(0x11223344, 32);
  auto status2 = chain.Schedule(std::move(tx));

  tms_dest = BitStream(reinterpret_cast<uint8_t *>(&tms), 8 * sizeof(tms));
  tdi_dest = BitStream(reinterpret_cast<uint8_t *>(&tdi), 8 * sizeof(tdi));

  pulled = jtag.PullTMSTDI(tms_dest, tdi_dest);

  EXPECT_EQ(pulled, 38);
  EXPECT_EQ(tms, (1ull << (31 + 2 + 4)) | 0b0011);
  EXPECT_EQ(tdi, 0x11223344ull << 6);

  tdo = 0x55667788ull << 6;
  tdo_src = BitStream(reinterpret_cast<const uint8_t *>(&tdo), pulled);

  jtag.PushTDO(tdo_src);

  ASSERT_TRUE(status2.done());
  ASSERT_TRUE(*status2);

  auto complete = status2->Complete();
  auto it = complete.begin();
  ASSERT_NE(it, complete.end());

  auto *shiftdr_out = it->Out<JCShiftDR>();
  ASSERT_NE(shiftdr_out, nullptr);

  ASSERT_EQ(0x55667788, shiftdr_out->Bits().Read<uint64_t>(32));
}
