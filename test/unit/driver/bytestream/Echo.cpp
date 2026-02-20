#include "Mocks.h"
#include "test/unit/driver/Mocks.h"

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/Error.hpp"
#include "libedr/driver/bytestream/ByteStreamAction.hpp"

#include <bit>
#include <cstddef>
#include <gtest/gtest.h>
#include <span>

using namespace edr;

static std::vector<uint8_t>
VectorFromBytes(const std::span<const std::byte> &bytes) {
  std::span<const uint8_t> u8span(
      reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
  return {u8span.begin(), u8span.end()};
}

TEST(ByteStreamEcho, finishes_immediately_if_enough_data) {
  MockLoggerOutput logger_output(LogLevel::ERROR);
  Logger logger(logger_output, LogLevel::TRACE);
  DriverContext ctx(logger);
  MockPipe pipe;
  MockPipeByteStream stream(ctx, "Echo", pipe, pipe);

  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6};

  auto xact = stream.Initiate("Write, then read");
  auto pwrite = xact.Add<WriteBytes>(std::as_bytes(std::span<uint8_t>(data)));
  auto pread1 = xact.Add<ReadBytes>(1);
  auto pread2 = xact.Add<ReadBytes>(2);
  auto pread3 = xact.Add<ReadBytes>(3);

  auto task = stream.Schedule(std::move(xact));

  EXPECT_TRUE(task.done());

  auto status = task.TakeResult();
  EXPECT_TRUE(status);

  auto *wout = status.Out<WriteBytes>(pwrite);
  auto *rout1 = status.Out<ReadBytes>(pread1);
  auto *rout2 = status.Out<ReadBytes>(pread2);
  auto *rout3 = status.Out<ReadBytes>(pread3);

  EXPECT_EQ(wout->num_written, data.size());

  std::vector<uint8_t> expected1(data.begin(), data.begin() + 1);
  auto actual1 = VectorFromBytes(rout1->Span());
  EXPECT_EQ(actual1, expected1);

  std::vector<uint8_t> expected2(data.begin() + 1, data.begin() + 3);
  auto actual2 = VectorFromBytes(rout2->Span());
  EXPECT_EQ(actual2, expected2);

  std::vector<uint8_t> expected3(data.begin() + 3, data.end());
  auto actual3 = VectorFromBytes(rout3->Span());
  EXPECT_EQ(actual3, expected3);
}

TEST(ByteStreamEcho, can_suspend_until_enough_data_appears) {
  MockLoggerOutput logger_output(LogLevel::ERROR);
  Logger logger(logger_output, LogLevel::TRACE);
  DriverContext ctx(logger);
  MockPipe pipe;
  MockPipeByteStream stream(ctx, "Echo", pipe, pipe);

  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6};

  auto xact1 = stream.Initiate("Write, then read too much");
  auto pwrite1 = xact1.Add<WriteBytes>(std::as_bytes(std::span<uint8_t>(data)));
  auto pread1 = xact1.Add<ReadBytes>(3);
  auto pread2 = xact1.Add<ReadBytes>(10);

  auto task1 = stream.Schedule(std::move(xact1));

  EXPECT_FALSE(task1.done());

  auto xact2 = stream.Initiate("Write the rest");
  auto pwrite2 = xact2.Add<WriteBytes>(std::as_bytes(std::span<uint8_t>(data)));
  auto pwrite3 = xact2.Add<WriteBytes>(std::as_bytes(std::span<uint8_t>(data)));

  auto task2 = stream.Schedule(std::move(xact2));

  EXPECT_TRUE(task2.done());
  EXPECT_TRUE(task1.done());

  auto status1 = task1.TakeResult();
  EXPECT_TRUE(status1);

  auto *rout2 = status1.Out<ReadBytes>(pread2);

  std::vector<uint8_t> expected2 = {4, 5, 6, 1, 2, 3, 4, 5, 6, 1};
  auto actual2 = VectorFromBytes(rout2->Span());
  EXPECT_EQ(actual2, expected2);
}

TEST(ByteStreamEcho, can_report_unsupported_actions) {
  MockLoggerOutput logger_output(LogLevel::ERROR);
  Logger logger(logger_output, LogLevel::TRACE);
  DriverContext ctx(logger);
  MockPipe pipe;
  MockPipeByteStream stream(ctx, "Echo", pipe, pipe);

  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6};

  auto xact = stream.Initiate("Send an invalid action");
  auto pwrite = xact.Add<WriteBytes>(std::as_bytes(std::span<uint8_t>(data)));
  auto pread1 = xact.Add<ReadBytes>(1);
  auto pread2 = xact.Add<ReadBytes>(2);
  auto pread3 = xact.Add<ReadBytes>(3);

  auto *pread2_action = reinterpret_cast<ByteStreamAction *>(
      reinterpret_cast<std::byte *>(pread2.position.first.chunk) +
      pread2.position.first.offset);

  auto invalid_action_id = std::bit_cast<ActionID>(0xffffffff);

  const_cast<ActionID &>(pread2_action->discriminant) = invalid_action_id;

  EXPECT_CALL(logger_output,
              CheckLine(LogLevel::ERROR,
                        "[ERROR] [(Echo) Send an invalid action] "
                        "Unknown or corrupted action with ID 4294967295"));

  auto task = stream.Schedule(std::move(xact));

  EXPECT_TRUE(task.done());

  auto status = task.TakeResult();
  EXPECT_FALSE(status);

  EXPECT_NE(nullptr, status.CauseAs<CauseUnsupportedAction>());
  EXPECT_EQ(nullptr, status.FailedAction());
  EXPECT_EQ(invalid_action_id, status.FailedActionID());

  auto incomplete = status.Incomplete();
  auto complete = status.Complete();

  EXPECT_EQ(complete.begin(), complete.end());
  EXPECT_EQ(incomplete.begin(), status.begin());
  EXPECT_EQ(incomplete.end(), status.end());
}

TEST(ByteStreamEcho, survives_builder_allocation_failure) {
  MockLoggerOutput logger_output(LogLevel::ERROR);
  Logger logger(logger_output, LogLevel::TRACE);

  DisableableMemoryResource mr;

  DriverContext ctx{
      .logger = logger,
      .transaction_buffer_resource = &mr,
      .task_frame_resource = &mr,
  };

  MockPipe pipe;
  MockPipeByteStream stream(ctx, "Echo", pipe, pipe);

  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6};

  mr.Disable();

  auto xact = stream.Initiate("Write, then read");

  auto pwrite = xact.Add<WriteBytes>(std::as_bytes(std::span<uint8_t>(data)));
  auto pread1 = xact.Add<ReadBytes>(1);
  auto pread2 = xact.Add<ReadBytes>(2);
  auto pread3 = xact.Add<ReadBytes>(3);

  mr.Enable();

  EXPECT_CALL(logger_output,
              CheckLine(LogLevel::ERROR,
                        "[ERROR] [(Echo) Write, then read] "
                        "Invalid transaction input: Allocation failure"));

  auto task = stream.Schedule(std::move(xact));

  EXPECT_TRUE(task.done());

  auto status = task.TakeResult();
  EXPECT_FALSE(status);
}

TEST(ByteStreamEcho,
     survives_task_frame_allocation_failure_and_task_is_joinable) {
  MockLoggerOutput logger_output(LogLevel::ERROR);
  Logger logger(logger_output, LogLevel::TRACE);

  DisableableMemoryResource mr;

  DriverContext ctx{
      .logger = logger,
      .transaction_buffer_resource = &mr,
      .task_frame_resource = &mr,
  };

  MockPipe pipe;
  MockPipeByteStream stream(ctx, "Echo", pipe, pipe);

  auto xact = stream.Initiate("Read something");
  xact.Add<ReadBytes>(1);

  mr.Disable();

  EXPECT_CALL(logger_output,
              CheckLine(LogLevel::ERROR,
                        "[ERROR] [(Echo) Read something] "
                        "Failed to allocate a task frame, dropping"));

  auto task = stream.Schedule(std::move(xact));

  mr.Enable();

  stream.Join(task);

  EXPECT_TRUE(task.done());

  auto mb_status = task.TakeResult();
  EXPECT_FALSE(mb_status);
}

TEST(ByteStreamEcho, supports_big_actions) {
  MockLoggerOutput logger_output(LogLevel::ERROR);
  Logger logger(logger_output, LogLevel::TRACE);
  DriverContext ctx(logger);
  MockPipe pipe;
  MockPipeByteStream stream(ctx, "Echo", pipe, pipe);

  auto chunk_size = g_min_transaction_data_chunk_size;

  std::vector<uint8_t> data1;
  for (size_t i = 0; i < chunk_size * 2; i++)
    data1.push_back(i);

  std::vector<uint8_t> data2;
  for (size_t i = chunk_size * 2; i < chunk_size * 5; i++)
    data2.push_back(i);

  auto xact = stream.Initiate("Write big chunks, read big chunks");
  xact.Add<WriteBytes>(std::as_bytes(std::span<uint8_t>(data1)));
  xact.Add<WriteBytes>(std::as_bytes(std::span<uint8_t>(data2)));
  xact.Add<ReadBytes>(chunk_size * 3);
  xact.Add<ReadBytes>(chunk_size * 2);

  auto task = stream.Schedule(std::move(xact));

  ASSERT_TRUE(task.done());

  auto status = task.TakeResult();
  ASSERT_TRUE(status);

  auto it = status.Complete().begin();
  auto end = status.Complete().end();

  ASSERT_NE(it, end);
  auto *wout1 = it->Out<WriteBytes>();
  ASSERT_NE(nullptr, wout1);
  EXPECT_EQ(data1.size(), wout1->num_written);
  it++;

  ASSERT_NE(it, end);
  auto *wout2 = it->Out<WriteBytes>();
  ASSERT_NE(nullptr, wout2);
  EXPECT_EQ(data2.size(), wout2->num_written);
  it++;

  ASSERT_NE(it, end);
  auto *rout1 = it->Out<ReadBytes>();
  ASSERT_NE(nullptr, rout1);
  auto actual1 = VectorFromBytes(rout1->Span());

  std::vector<uint8_t> expected1;
  for (size_t i = 0; i < chunk_size * 3; i++)
    expected1.push_back(i);

  EXPECT_EQ(expected1, actual1);
  it++;

  ASSERT_NE(it, end);
  auto *rout2 = it->Out<ReadBytes>();
  ASSERT_NE(nullptr, rout2);
  auto actual2 = VectorFromBytes(rout2->Span());

  std::vector<uint8_t> expected2;
  for (size_t i = chunk_size * 3; i < chunk_size * 5; i++)
    expected2.push_back(i);

  EXPECT_EQ(expected2, actual2);
  it++;

  EXPECT_EQ(it, end);
}

TEST(ByteStreamEcho, supports_multiple_actions_smaller_than_chunk) {
  MockLoggerOutput logger_output(LogLevel::ERROR);
  Logger logger(logger_output, LogLevel::TRACE);
  DriverContext ctx(logger);
  MockPipe pipe;
  MockPipeByteStream stream(ctx, "Echo", pipe, pipe);

  auto xact = stream.Initiate("Write many small chunks, read one big");

  auto start_chunk_size = g_min_transaction_data_chunk_size * 2 / 5;
  auto num_chunks = 6;

  auto total = 0;

  for (size_t i = 0; i < num_chunks; i++) {
    auto chunk_size = start_chunk_size + i;
    std::vector<uint8_t> data;
    for (size_t j = total; j < total + chunk_size; j++)
      data.push_back(j);

    xact.Add<WriteBytes>(std::as_bytes(std::span<uint8_t>(data)));
  }

  xact.Add<ReadBytes>(total);

  auto task = stream.Schedule(std::move(xact));

  ASSERT_TRUE(task.done());

  auto status = task.TakeResult();
  ASSERT_TRUE(status);

  auto it = status.Complete().begin();
  auto end = status.Complete().end();

  for (size_t i = 0; i < num_chunks; i++) {
    ASSERT_NE(it, end);
    auto *wout = it->Out<WriteBytes>();
    ASSERT_NE(nullptr, wout);

    auto chunk_size = start_chunk_size + i;
    EXPECT_EQ(chunk_size, wout->num_written);
    it++;
  }

  ASSERT_NE(it, end);
  auto *rout = it->Out<ReadBytes>();
  ASSERT_NE(nullptr, rout);
  auto actual = VectorFromBytes(rout->Span());

  std::vector<uint8_t> expected;
  for (size_t i = 0; i < total; i++)
    expected.push_back(i);

  EXPECT_EQ(expected, actual);
  it++;

  EXPECT_EQ(it, end);
}

TEST(ByteStreamEcho, logs_successful_transactions) {
  MockLoggerOutput logger_output(LogLevel::TRACE);

  std::array<const char *, 7> lines = {
      "[TRACE] Initiating (Echo) tx",
      "[TRACE] [(Echo) tx] Scheduling",
      "[TRACE] | WRITE 6 [01 02 03 04 05 06]",
      "[TRACE] | READ 3",
      "[TRACE] [(Echo) tx] Completed",
      "[TRACE] | WRITE 6 [01 02 03 04 05 06] -> 6",
      "[TRACE] | READ 3 -> 3 [01 02 03]",
  };

  for (const auto &line : lines)
    EXPECT_CALL(logger_output, CheckLine(LogLevel::TRACE, line)).Times(1);

  Logger logger(logger_output, LogLevel::TRACE);
  DriverContext ctx(logger);
  MockPipe pipe;
  MockPipeByteStream stream(ctx, "Echo", pipe, pipe);

  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6};

  auto xact = stream.Initiate("tx");
  xact.Add<WriteBytes>(std::as_bytes(std::span<uint8_t>(data)));
  xact.Add<ReadBytes>(3);

  auto task = stream.Schedule(std::move(xact));
}

TEST(ByteStreamEcho, logs_errors) {
  MockLoggerOutput logger_output(LogLevel::TRACE);

  std::array<const char *, 8> lines = {
      "[TRACE] Initiating (Echo) tx",
      "[TRACE] [(Echo) tx] Scheduling",
      "[TRACE] | WRITE 6 [01 02 03 04 05 06]",
      "[TRACE] | READ 2",
      "[TRACE] | READ 3",
      "[TRACE] [(Echo) tx] Completed",
      "[TRACE] | WRITE 6 [01 02 03 04 05 06] -> 6",
      "[TRACE] | READ 2 => FAILED, cause: Injected error",
  };

  for (const auto &line : lines)
    EXPECT_CALL(logger_output, CheckLine(LogLevel::TRACE, line)).Times(1);

  Logger logger(logger_output, LogLevel::TRACE);
  DriverContext ctx(logger);
  MockPipe pipe;
  MockPipeByteStream stream(ctx, "Echo", pipe, pipe);

  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6};

  auto xact = stream.Initiate("tx");
  xact.Add<WriteBytes>(std::as_bytes(std::span<uint8_t>(data)));
  xact.Add<ReadBytes>(2);
  xact.Add<ReadBytes>(3);

  stream.FailAfter(1);

  auto task = stream.Schedule(std::move(xact));
}
