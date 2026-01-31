#include "libedr/util/adt/BitStream.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace edr;
using namespace testing;

TEST(BitStream, can_write_and_read_different_sizes) {
  uint32_t storage[16];
  BitStream writer(storage, 0);

  writer.Write<uint32_t>(0x11223344u, 32);
  writer.Write<uint32_t>(0xaabbccddu, 16);
  writer.Write<uint8_t>(0xff, 8);
  writer.Write<uint64_t>(0x1122334455667788, 64);

  uint32_t result[16];
  memcpy(result, storage, sizeof(result));

  BitStream reader(result);
  uint64_t part1 = reader.Read<uint64_t>(64);
  uint32_t part2 = reader.Read<uint32_t>(24);
  uint16_t part3 = reader.Read<uint16_t>(16);
  uint16_t part4 = reader.Read<uint16_t>(8);
  uint8_t part5 = reader.Read<uint8_t>(8);

  EXPECT_EQ(0x88ffccdd11223344u, part1);
  EXPECT_EQ(0x556677u, part2);
  EXPECT_EQ(0x3344u, part3);
  EXPECT_EQ(0x22u, part4);
  EXPECT_EQ(0x11u, part5);
}

TEST(BitStream, can_write_aligned_streams) {
  constexpr size_t num_chunks = 16;
  uint32_t storage1[num_chunks];
  for (size_t i = 0; i < num_chunks; i++)
    storage1[i] = (i << 24) + (i << 16) + (i << 8) + i;

  uint16_t storage2[2 * num_chunks];
  BitStream source(storage1);
  BitStream writer(storage2);

  writer.Write(source, 8 * sizeof(storage1[0]) * num_chunks);

  uint32_t result[num_chunks];
  memcpy(result, storage2, sizeof(result));

  BitStream reader(result);
  for (size_t i = 0; i < num_chunks; i++)
    EXPECT_EQ(storage1[i], reader.Read<uint32_t>(32));
}

TEST(BitStream, can_write_misaligned_streams) {
  constexpr size_t num_chunks = 16;
  uint32_t storage1[num_chunks];
  for (size_t i = 0; i < num_chunks; i++)
    storage1[i] = (i << 24) + (i << 16) + (i << 8) + i;

  uint16_t storage2[2 * num_chunks];
  BitStream source(storage1, 16);
  BitStream writer(storage2, 8);

  writer.Write(source, 8 * sizeof(storage1[0]) * num_chunks - 16);

  uint32_t result[num_chunks];
  memcpy(result, storage2, sizeof(result));

  BitStream reader(result, 8);
  for (size_t i = 0; i < num_chunks - 1; i++)
    EXPECT_EQ(((i + 1) << 24) + ((i + 1) << 16) + (i << 8) + i,
              reader.Read<uint32_t>(32));

  auto last = reader.Read<uint32_t>(16);
  EXPECT_EQ((num_chunks - 1) + ((num_chunks - 1) << 8), last);
}
