#include "Mocks.h"

#include "libedr/util/vss/VSS.hpp"

#include <gtest/gtest.h>
#include <tuple>
#include <vector>

using namespace edr;
using namespace testing;

struct TrivialBoundA4 {
  uint32_t a = 0xffeeddcc;
  float b = 20.7f;
  char c = 'd';
  char padding[3] = {};
};

struct TrivialBoundA8 {
  TrivialBoundA8(uint64_t d) : d(d) {}

  TrivialBoundA8(uint32_t dh, uint32_t dl)
      : d((static_cast<uint64_t>(dh) << 32u) | dl) {}

  uint64_t d;
};

struct TrivialVSS {
  TrivialVSS(size_t size) : data_size(size) {}

  uint32_t data_size;

  using Payload = vss::Payload<>;

  std::span<uint8_t> GetData() {
    return {reinterpret_cast<uint8_t *>(this) + sizeof(*this), data_size};
  }

  static void EmplacePayload(vss::OutputStream auto &out, size_t size) {
    auto [_, data] = out.Allocate(size);
    if (nullptr == data)
      return;

    auto *u8data = reinterpret_cast<uint8_t *>(data);
    for (size_t i = 0; i < size; i++)
      *(u8data++) = i + 1;
  }

  std::optional<size_t> SizeOfPayload(Payload & /*payload*/,
                                      size_t /*max_size*/) {
    return data_size;
  }

  TrivialVSS(const TrivialVSS &) = delete;
  TrivialVSS(TrivialVSS &&) = delete;
};

static_assert(vss::HasPayload<TrivialVSS>);

struct alignas(8) TrivialVSS2Args {
  TrivialVSS2Args(size_t size1, size_t size2) : data_size(size1 + size2) {}

  uint32_t data_size;

  std::span<uint8_t> GetData() {
    return {reinterpret_cast<uint8_t *>(this) + sizeof(*this), data_size};
  }

  using Payload = vss::Payload<>;

  static void EmplacePayload(vss::OutputStream auto &os, size_t size1,
                             size_t size2) {
    size_t size = size1 + size2;

    auto [_, data] = os.Allocate(size);
    if (nullptr == data)
      return;

    auto *u8data = reinterpret_cast<uint8_t *>(data);
    for (size_t i = 0; i < size; i++)
      *(u8data++) = i + 1;
  }

  std::optional<size_t> SizeOfPayload(Payload & /*payload*/,
                                      size_t /*max_size*/) {
    return data_size;
  }

  TrivialVSS2Args(const TrivialVSS2Args &) = delete;
  TrivialVSS2Args(TrivialVSS2Args &&) = delete;
};

static_assert(vss::HasPayload<TrivialVSS2Args>);

struct CompoundVSS {
  CompoundVSS(uint32_t a, uint32_t b, size_t size0, size_t size1, size_t size2)
      : a(a), b(b) {}

  uint32_t a;
  uint32_t b;

  using Payload = vss::Payload<TrivialVSS, TrivialVSS2Args>;

  static void EmplacePayload(vss::OutputStream auto &os, uint32_t a, uint32_t b,
                             size_t size0, size_t size1, size_t size2) {
    Payload::Emplace(os, size0, std::forward_as_tuple(size1, size2));
  }

  std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size) {
    return payload.Size(max_size);
  }

  CompoundVSS(const CompoundVSS &) = delete;
  CompoundVSS(CompoundVSS &&) = delete;
};

static_assert(vss::HasPayload<CompoundVSS>);

TEST(VSS, vss_are_placed_consecutively_and_aligned) {
  VSSOutVector out;

  size_t a4pos = vss::Emplace<TrivialBoundA4>(out, 0x11223344, 10.3f, 'a');
  size_t a8pos = vss::Emplace<TrivialBoundA8>(out, 0x5566778899aabbcc);

  EXPECT_EQ(0, a4pos % alignof(TrivialBoundA4));
  EXPECT_EQ(0, a8pos % alignof(TrivialBoundA8));

  std::vector<uint8_t> expected = {
      // TrivialBoundA4
      0x44, 0x33, 0x22, 0x11, 0xcd, 0xcc, 0x24, 0x41, 'a', 0, 0, 0,
      // Padding
      0, 0, 0, 0,
      // TrivialBoundA8
      0xcc, 0xbb, 0xaa, 0x99, 0x88, 0x77, 0x66, 0x55};
  EXPECT_EQ(expected, out.Data());
}

TEST(VSS, trivial_bound_vss_can_be_constructed_in_multiple_ways) {
  VSSOutVector out1;
  VSSOutVector out2;
  VSSOutVector out3;
  VSSOutVector out4;
  VSSOutVector out5;

  vss::Emplace<TrivialBoundA4>(out1, 0x11223344, 10.3f, 'a');
  vss::Emplace<TrivialBoundA4>(
      out2, TrivialBoundA4{.a = 0x11223344, .b = 10.3f, .c = 'a'});

  vss::Emplace<TrivialBoundA4>(out3);

  vss::Emplace<TrivialBoundA8>(out4, 0x11223344, 0x55667788);
  vss::Emplace<TrivialBoundA8>(out5, TrivialBoundA8(0x11223344, 0x55667788));

  EXPECT_EQ(out1.Data(), out2.Data());

  std::vector<uint8_t> expected3 = {0xcc, 0xdd, 0xee, 0xff, 0x9a, 0x99,
                                    0xa5, 0x41, 'd',  0,    0,    0};
  EXPECT_EQ(expected3, out3.Data());

  std::vector<uint8_t> expected4 = {0x88, 0x77, 0x66, 0x55,
                                    0x44, 0x33, 0x22, 0x11};
  EXPECT_EQ(expected4, out4.Data());
  EXPECT_EQ(out4.Data(), out5.Data());
}

TEST(VSS, trivial_unbound_vss_can_be_constructed) {
  VSSOutVector out;
  vss::Emplace<TrivialVSS>(out, 5);

  std::vector<uint8_t> expected = {0x5, 0, 0, 0, 1, 2, 3, 4, 5, 0, 0, 0};

  EXPECT_EQ(expected, out.Data());
}

TEST(VSS, compound_vss_can_be_constructed) {
  VSSOutVector out;

  vss::Emplace<CompoundVSS>(out, 0x11223344, 0x55667788, 3, 3, 7);

  std::vector<uint8_t> expected = {// CompoundVSS::a
                                   0x44, 0x33, 0x22, 0x11,
                                   // CompoundVSS::b
                                   0x88, 0x77, 0x66, 0x55,
                                   // TrivialVSS::size
                                   0x3, 0, 0, 0,
                                   // Data1
                                   1, 2, 3,
                                   // Padding
                                   0, 0, 0, 0, 0,
                                   // TrivialVSS::size
                                   0xa, 0, 0, 0,
                                   // Padding
                                   0, 0, 0, 0,
                                   // Data2
                                   0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9,
                                   0xa, 0, 0};

  EXPECT_EQ(expected, out.Data());

  auto *ptr = reinterpret_cast<CompoundVSS *>(out.Data().data());
  EXPECT_EQ(vss::SizeOfTrusted(*ptr), 38);
}

TEST(VSS, compound_vss_can_be_extracted) {
  VSSOutVector out;

  vss::Emplace<CompoundVSS>(out, 0x11223344, 0x55667788, 3, 3, 7);

  vss::ContiguousInputStream<VSSOutVector::g_alignment> in(out.Data().data(),
                                                           out.Data().size());

  auto *compound = vss::Extract<CompoundVSS>(in);
  ASSERT_NE(nullptr, compound);

  EXPECT_EQ(in.GetOffset(), out.Data().size());
  EXPECT_EQ(compound->a, 0x11223344);
  EXPECT_EQ(compound->b, 0x55667788);

  auto &vss1 = vss::Get<0>(*compound);
  auto &vss2 = vss::Get<1>(*compound);

  std::vector<uint8_t> actual1(vss1.GetData().begin(), vss1.GetData().end());
  std::vector<uint8_t> expected1 = {1, 2, 3};
  EXPECT_EQ(actual1, expected1);

  std::vector<uint8_t> actual2(vss2.GetData().begin(), vss2.GetData().end());
  std::vector<uint8_t> expected2 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  EXPECT_EQ(actual2, expected2);
}
