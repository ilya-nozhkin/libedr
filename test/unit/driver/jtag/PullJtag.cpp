#include "libedr/driver/jtag/JtagAction.hpp"
#include "test/unit/driver/Mocks.h"

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/jtag/PullJtag.h"

#include <cstdint>
#include <gtest/gtest.h>

using namespace edr;

TEST(PullJtag, generates_tms_tdi) {
  MockLoggerOutput logger_output(LogLevel::ERROR);
  Logger logger(logger_output, LogLevel::TRACE);
  DriverContext ctx(logger);
  PullJtag jtag(ctx, "JTAG");

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

  size_t pulled = jtag.PullTMSTDI(tms_dest, tdi_dest);

  EXPECT_EQ(pulled, 32);
  EXPECT_EQ(tms, 0x0007F123u);
  EXPECT_EQ(tdi, 0x7FF80000u);

  uint64_t tms2 = 0;
  uint64_t tdi2 = 0;
  tms_dest = BitStream(reinterpret_cast<uint8_t *>(&tms2), 8 * sizeof(tms2));
  tdi_dest = BitStream(reinterpret_cast<uint8_t *>(&tdi2), 8 * sizeof(tdi2));

  pulled = jtag.PullTMSTDI(tms_dest, tdi_dest);

  EXPECT_EQ(pulled, 48);
  EXPECT_EQ(tms2, 1ull << 47);
  EXPECT_EQ(tdi2, (0x54321ull << 28) | (0x12345678FFFull >> 13));
}

TEST(PullJtag, accepts_tdo) {
  MockLoggerOutput logger_output(LogLevel::ERROR);
  Logger logger(logger_output, LogLevel::TRACE);
  DriverContext ctx(logger);
  PullJtag jtag(ctx, "JTAG");

  auto tx = jtag.Initiate("tx");
  auto put_tms1 = tx.Add<PutTMS>(BitStream(0x123u, 12));
  auto put_tms2 = tx.Add<PutTMS>(BitStream(0xFFFu, 7));
  auto put_tdi_get_tdo = tx.Add<PutTDIGetTDO>(BitStream(0x54321u, 20), 1);
  auto put_tdi = tx.Add<PutTDI>(BitStream(0x12345678FFFull, 41), 0);

  auto task = jtag.Schedule(std::move(tx));

  uint32_t tms = 0;
  uint32_t tdi = 0;

  BitStream tms_dest(reinterpret_cast<uint8_t *>(&tms), 8 * sizeof(tms));
  BitStream tdi_dest(reinterpret_cast<uint8_t *>(&tdi), 8 * sizeof(tdi));

  size_t pulled = jtag.PullTMSTDI(tms_dest, tdi_dest);

  uint32_t tdo = 0x12345678u;
  BitStream<const uint8_t> tdo_source(reinterpret_cast<uint8_t *>(&tdo),
                                      8 * sizeof(tdo));
  size_t pushed = jtag.PushTDO(tdo_source);

  EXPECT_EQ(tdo_source.GetNumBits(), 0);
  EXPECT_EQ(pushed, 32);
  EXPECT_FALSE(task.done());

  uint64_t tms2 = 0;
  uint64_t tdi2 = 0;
  tms_dest = BitStream(reinterpret_cast<uint8_t *>(&tms2), 8 * sizeof(tms2));
  tdi_dest = BitStream(reinterpret_cast<uint8_t *>(&tdi2), 8 * sizeof(tdi2));

  pulled = jtag.PullTMSTDI(tms_dest, tdi_dest);

  uint64_t tdo2 = 0x0123456789ABCDEFull;
  tdo_source = BitStream<const uint8_t>(reinterpret_cast<uint8_t *>(&tdo2), 48);
  pushed = jtag.PushTDO(tdo_source);

  EXPECT_EQ(tdo_source.GetNumBits(), 0);
  EXPECT_EQ(pushed, 48);
  EXPECT_TRUE(task.done());

  auto tms_out1 = task->Out(put_tms1);
  auto tms_out2 = task->Out(put_tms2);
  auto tdo_out = task->Out(put_tdi_get_tdo);
  auto tdi_out = task->Out(put_tdi);

  EXPECT_EQ(tms_out1->num_put, 12);
  EXPECT_EQ(tms_out2->num_put, 7);

  auto tdo_bits = tdo_out->Bits();
  EXPECT_EQ(tdo_bits.GetNumBits(), 20);

  auto tdo_value = tdo_bits.Read<uint32_t>(20);
  EXPECT_EQ(tdo_value, ((tdo2 & ((1ull << 7ull) - 1ull)) << 13ull) |
                           (static_cast<uint64_t>(tdo) >> 19ull));

  EXPECT_EQ(tdi_out->num_put, 41);
}
