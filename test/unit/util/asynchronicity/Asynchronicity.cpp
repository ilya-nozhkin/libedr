#include "Mocks.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace edr;
using namespace testing;

TEST(Asynchronicity, uses_allocator_from_the_asynchronous) {
  MockAllocator allocator;
  Endpoint endpoint;
  MockAsynchronous<MockAllocatorProxy> asynchronous(
      endpoint, MockAllocatorProxy(allocator));

  std::size_t allocated_size = 0;
  void *allocated_ptr = nullptr;

  std::size_t freed_size = 0;
  void *freed_ptr = nullptr;

  EXPECT_CALL(allocator, allocate)
      .Times(Exactly(1))
      .WillOnce(Invoke([&](size_t size) {
        allocated_size = size;
        allocated_ptr = malloc(size);
        return allocated_ptr;
      }));

  EXPECT_CALL(allocator, deallocate)
      .Times(Exactly(1))
      .WillOnce(Invoke([&](void *ptr, size_t size) {
        freed_size = size;
        freed_ptr = ptr;
        free(ptr);
      }));

  {
    asynchronous.TrivialEndpoint();
  }

  EXPECT_NE(allocated_size, 0);
  EXPECT_NE(allocated_ptr, nullptr);

  EXPECT_EQ(allocated_size, freed_size);
  EXPECT_EQ(allocated_ptr, freed_ptr);
}

TEST(Asynchronicity, memory_is_not_freed_until_the_return_object_is_destroyed) {
  MockAllocator allocator;
  Endpoint endpoint;
  MockAsynchronous<MockAllocatorProxy> asynchronous(
      endpoint, MockAllocatorProxy(allocator));

  bool freed = false;

  EXPECT_CALL(allocator, allocate)
      .Times(Exactly(1))
      .WillOnce(Invoke([&](size_t size) { return malloc(size); }));

  EXPECT_CALL(allocator, deallocate)
      .Times(Exactly(1))
      .WillOnce(Invoke([&](void *ptr, size_t size) {
        free(ptr);
        freed = true;
      }));

  {
    auto task = asynchronous.TrivialEndpoint();
    EXPECT_FALSE(freed);
  }

  EXPECT_TRUE(freed);
}

TEST(Asynchronicity, allocation_failures_are_taken_into_account) {
  MockAllocator allocator;
  Endpoint endpoint;
  MockAsynchronous<MockAllocatorProxy> asynchronous(
      endpoint, MockAllocatorProxy(allocator));

  std::size_t allocated_size = 0;

  EXPECT_CALL(allocator, allocate)
      .Times(Exactly(1))
      .WillOnce(Invoke([&](size_t size) {
        allocated_size = size;
        return nullptr;
      }));

  EXPECT_CALL(allocator, deallocate).Times(Exactly(0));

  {
    auto task = asynchronous.TrivialEndpoint();
    EXPECT_FALSE(task);
  }

  EXPECT_NE(allocated_size, 0);
}

TEST(Asynchronicity, trivials_finish_immediately) {
  Endpoint endpoint;
  MockAsynchronous asynchronous(endpoint);

  EXPECT_CALL(asynchronous, TrivialDependentFinished).Times(Exactly(1));

  asynchronous.TrivialDependent();
}

TEST(Asynchronicity, resolvable_initiates_immediately) {
  Endpoint endpoint;
  MockAsynchronous asynchronous(endpoint);

  auto task = asynchronous.Resolvable();

  EXPECT_TRUE(endpoint.EmptyResolverIsPending());

  task.destroy();
}

TEST(Asynchronicity, resolvables_wait_until_resolved) {
  Endpoint endpoint;
  MockAsynchronous asynchronous(endpoint);

  bool resolvable_finished = false;
  bool dependent_finished = false;

  Expectation exp_resolvable =
      EXPECT_CALL(asynchronous, ResolvableFinished)
          .Times(1)
          .WillOnce(Assign(&resolvable_finished, true));

  EXPECT_CALL(asynchronous, ResolvableDependentFinished)
      .Times(1)
      .After(exp_resolvable)
      .WillOnce(Assign(&dependent_finished, true));

  {
    asynchronous.ResolvableDependent();

    EXPECT_FALSE(resolvable_finished);
    EXPECT_FALSE(dependent_finished);

    endpoint.ResolveEmpty();

    EXPECT_TRUE(resolvable_finished);
    EXPECT_TRUE(dependent_finished);
  }
}

static constexpr char g_payload[] =
    "some really long string that cannot be stored in-place using the small "
    "string optimization";

TEST(Asynchronicity, result_can_be_extracted_after_suspension) {
  ResultListener listener;

  Endpoint endpoint;
  MockAsynchronous asynchronous(endpoint);

  {
    auto task = asynchronous.ResolvableWithResult();

    EXPECT_TRUE(endpoint.ResultResolverIsPending());
    EXPECT_FALSE(task.done());

    bool destroyed = false;

    EXPECT_CALL(listener, Constructor).Times(Exactly(1));
    EXPECT_CALL(listener, Copy).Times(Exactly(0));
    EXPECT_CALL(listener, Move).Times(AnyNumber());
    EXPECT_CALL(listener, FinalDestructor)
        .Times(Exactly(1))
        .WillOnce(Assign(&destroyed, true));

    endpoint.Resolve(listener, g_payload);

    EXPECT_TRUE(task.done());
    EXPECT_FALSE(destroyed);

    auto result = task.TakeResult();
    EXPECT_EQ(g_payload, result.GetPayload());
  }
}

TEST(Asynchronicity, result_survives_preawait_resolution) {
  ResultListener listener;

  Endpoint endpoint;
  MockAsynchronous asynchronous(endpoint);

  {
    bool destroyed = false;

    EXPECT_CALL(listener, Constructor).Times(Exactly(1));
    EXPECT_CALL(listener, Copy).Times(Exactly(0));
    EXPECT_CALL(listener, Move).Times(AnyNumber());
    EXPECT_CALL(listener, FinalDestructor)
        .Times(Exactly(1))
        .WillOnce(Assign(&destroyed, true));

    auto task = asynchronous.ResolvingBeforeAwait(listener, g_payload);

    EXPECT_TRUE(task.done());
    EXPECT_FALSE(destroyed);

    auto result = task.TakeResult();
    EXPECT_EQ(g_payload, result.GetPayload());
  }
}
