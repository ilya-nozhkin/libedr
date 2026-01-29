#include "Mocks.h"

#include "libedr/util/vss/VSS.hpp"
#include "libedr/util/vss/VSSPayloads.hpp"

#include <gtest/gtest.h>
#include <vector>

using namespace edr;
using namespace testing;

TEST(VSSPayloads, dependent_array_can_be_constructed_consistently) {
  VSSOutVector out;

  std::vector<uint16_t> data = {1,      2, 3,      4,  5,     6,
                                0xaabb, 8, 0xccdd, 10, 0xeeff};

  using VSS = vss::DependentArray<uint16_t>;

  vss::Emplace<VSS>(out, data);

  vss::ContiguousInputStream<VSSOutVector::g_alignment> in(out.Data().data(),
                                                           out.Data().size());
  auto *vss = vss::Extract<VSS>(in, data.size());

  EXPECT_NE(nullptr, vss);
  EXPECT_EQ(in.GetOffset(), out.Data().size());

  std::vector<uint16_t> actual(vss->Data(), vss->Data() + data.size());
  EXPECT_EQ(data, actual);
}

struct WithDependentArray {
  WithDependentArray(const std::span<uint16_t> &data) : size(data.size()) {}

  uint32_t size;

  using Payload = vss::Payload<vss::DependentArray<uint16_t>>;

  auto Span() { return std::span<uint16_t>(vss::Get<0>(*this).Data(), size); }

  static void EmplacePayload(vss::OutputStream auto &os,
                             const std::span<uint16_t> &data) {
    Payload::Emplace(os, data);
  }

  std::optional<size_t> SizeOfPayload(Payload &payload, size_t max_size) {
    return payload.Size(max_size, size);
  }

  WithDependentArray(const WithDependentArray &) = delete;
  WithDependentArray(WithDependentArray &&) = delete;
};

TEST(VSSPayloads, dependent_array_can_be_nested) {
  VSSOutVector out;

  std::vector<uint16_t> data = {1,      2, 3,      4,  5,     6,
                                0xaabb, 8, 0xccdd, 10, 0xeeff};

  vss::Emplace<WithDependentArray>(out, data);

  vss::ContiguousInputStream<VSSOutVector::g_alignment> in(out.Data().data(),
                                                           out.Data().size());
  auto *vss = vss::Extract<WithDependentArray>(in);

  EXPECT_NE(nullptr, vss);
  EXPECT_EQ(in.GetOffset(), out.Data().size());

  std::vector<uint16_t> actual(vss->Span().begin(), vss->Span().end());
  EXPECT_EQ(data, actual);
}

TEST(VSSPayloads, trivial_variant_is_consistent) {
  using TrivialVariant = vss::Variant<int, double, bool>;
  using BiggerVariant = vss::Variant<int, double, bool, float>;

  VSSOutVector out;

  vss::Emplace<TrivialVariant::Option<int>>(out, 10);
  vss::Emplace<TrivialVariant::Option<double>>(out, 20.5);
  vss::Emplace<TrivialVariant::Option<bool>>(out, true);
  vss::Emplace<BiggerVariant::Option<float>>(out, 5.5f);

  vss::ContiguousInputStream<VSSOutVector::g_alignment> in(out.Data().data(),
                                                           out.Data().size());

  auto *v_int = vss::Extract<TrivialVariant>(in);
  auto *v_double = vss::Extract<TrivialVariant>(in);
  auto *v_bool = vss::Extract<TrivialVariant>(in);
  auto *v_float = vss::Extract<TrivialVariant>(in);

  EXPECT_NE(nullptr, v_int);
  EXPECT_NE(nullptr, v_double);
  EXPECT_NE(nullptr, v_bool);
  EXPECT_NE(nullptr, v_float);

  EXPECT_TRUE(static_cast<bool>(*v_int));
  EXPECT_TRUE(static_cast<bool>(*v_double));
  EXPECT_TRUE(static_cast<bool>(*v_bool));
  EXPECT_FALSE(static_cast<bool>(*v_float));

  EXPECT_NE(nullptr, v_int->As<int>());
  EXPECT_NE(nullptr, v_double->As<double>());
  EXPECT_NE(nullptr, v_bool->As<bool>());

  EXPECT_EQ(nullptr, v_int->As<double>());
  EXPECT_EQ(nullptr, v_int->As<bool>());
  EXPECT_EQ(nullptr, v_double->As<int>());
  EXPECT_EQ(nullptr, v_double->As<bool>());
  EXPECT_EQ(nullptr, v_bool->As<int>());
  EXPECT_EQ(nullptr, v_bool->As<double>());

  EXPECT_EQ(10, *v_int->As<int>());
  EXPECT_EQ(20.5, *v_double->As<double>());
  EXPECT_EQ(true, *v_bool->As<bool>());
}
