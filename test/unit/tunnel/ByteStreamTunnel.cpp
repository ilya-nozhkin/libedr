#include "test/unit/driver/bytestream/Mocks.h"

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/Error.hpp"
#include "libedr/driver/Logger.hpp"
#include "libedr/driver/bytestream/ByteStreamAction.hpp"
#include "libedr/tunnel/ByteStreamTunnel.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <span>

using namespace edr;

static std::vector<uint8_t>
VectorFromBytes(const std::span<const std::byte> &bytes) {
  std::span<const uint8_t> u8span(
      reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
  return {u8span.begin(), u8span.end()};
}

TEST(ByteStreamTunnel, forwards_successful_transactions) {
  MockLoggerOutput logger_output(LogLevel::ERROR);
  Logger logger(logger_output, LogLevel::TRACE);
  DriverContext ctx(logger);

  MockPipe loopback;

  MockPipe pipe1;
  MockPipe pipe2;

  MockPipeByteStream remote(ctx, "Echo", loopback, loopback);

  MockPipeByteStream server_stream(ctx, "Server", pipe1, pipe2);
  ByteStreamTunnel server(ctx, server_stream);
  server.RegisterDriver(remote);

  MockPipeByteStream client_stream(ctx, "Client", pipe2, pipe1);
  ByteStreamTunnel client(ctx, client_stream);

  auto server_handshake = server.Handshake();
  auto client_handshake = client.Handshake();

  ASSERT_TRUE(server_handshake.done());
  ASSERT_TRUE(client_handshake.done());

  auto server_handshake_error = server_handshake.TakeResult();
  auto client_handshake_error = client_handshake.TakeResult();

  ASSERT_FALSE(server_handshake_error);
  ASSERT_FALSE(client_handshake_error);

  auto *forwarding = client.FindByName<ByteStream>("Echo");
  ASSERT_NE(nullptr, forwarding);

  for (size_t i = 0; i < 3; i++) {
    std::vector<uint8_t> data;
    for (size_t j = 0; j < 6; j++)
      data.push_back(i + j);

    auto xact = forwarding->Initiate("Write, then read");
    auto pwrite = xact.Add<WriteBytes>(std::as_bytes(std::span<uint8_t>(data)));
    auto pread1 = xact.Add<ReadBytes>(2);
    auto pread2 = xact.Add<ReadBytes>(4);

    auto task = forwarding->Schedule(std::move(xact));

    EXPECT_TRUE(task.done());

    auto status = task.TakeResult();
    EXPECT_TRUE(status);

    auto *wout = status.Out<WriteBytes>(pwrite);
    auto *rout1 = status.Out<ReadBytes>(pread1);
    auto *rout2 = status.Out<ReadBytes>(pread2);

    EXPECT_EQ(wout->num_written, data.size());

    std::vector<uint8_t> expected1(data.begin(), data.begin() + 2);
    auto actual1 = VectorFromBytes(rout1->Span());
    EXPECT_EQ(actual1, expected1);

    std::vector<uint8_t> expected2(data.begin() + 2, data.end());
    auto actual2 = VectorFromBytes(rout2->Span());
    EXPECT_EQ(actual2, expected2);
  }
}

TEST(ByteStreamTunnel, forwards_errors) {
  MockLoggerOutput logger_output(LogLevel::ERROR);
  Logger logger(logger_output, LogLevel::TRACE);
  DriverContext ctx(logger);

  MockPipe loopback;

  MockPipe pipe1;
  MockPipe pipe2;

  MockPipeByteStream remote(ctx, "Echo", loopback, loopback);

  MockPipeByteStream server_stream(ctx, "Server", pipe1, pipe2);
  ByteStreamTunnel server(ctx, server_stream);
  server.RegisterDriver(remote);

  MockPipeByteStream client_stream(ctx, "Client", pipe2, pipe1);
  ByteStreamTunnel client(ctx, client_stream);

  auto server_handshake = server.Handshake();
  auto client_handshake = client.Handshake();

  ASSERT_TRUE(server_handshake.done());
  ASSERT_TRUE(client_handshake.done());

  auto server_handshake_error = server_handshake.TakeResult();
  auto client_handshake_error = client_handshake.TakeResult();

  ASSERT_FALSE(server_handshake_error);
  ASSERT_FALSE(client_handshake_error);

  auto *forwarding = client.FindByName<ByteStream>("Echo");
  ASSERT_NE(nullptr, forwarding);

  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6};

  auto xact = forwarding->Initiate("tx");
  xact.Add<WriteBytes>(std::as_bytes(std::span<uint8_t>(data)));
  xact.Add<ReadBytes>(2);
  xact.Add<ReadBytes>(3);

  remote.FailAfter(1);

  auto task = forwarding->Schedule(std::move(xact));

  ASSERT_TRUE(task.done());

  auto result = task.TakeResult();

  ASSERT_FALSE(result);

  auto it = result.Complete().begin();
  auto end = result.Complete().end();

  ASSERT_NE(it, end);

  it++;
  ASSERT_EQ(it, end);

  ActionError *error = result.GetError();
  ASSERT_NE(nullptr, error);

  AnyAction *failed_action = error->FailedAction();
  ASSERT_NE(nullptr, failed_action);

  auto *failed_rb = failed_action->As<ReadBytes>();
  ASSERT_NE(nullptr, failed_rb);

  ASSERT_EQ(2, failed_rb->size);

  auto *cause_message = error->GetCause().As<CauseStringMessage>();
  ASSERT_NE(nullptr, cause_message);
  ASSERT_EQ(cause_message->Message(), "Injected error");
}

TEST(ByteStreamTunnel, can_handle_corrupted_requests) {
  MockLoggerOutput logger_output(LogLevel::ERROR);
  Logger logger(logger_output, LogLevel::TRACE);
  DriverContext ctx(logger);

  MockPipe loopback;

  MockPipe pipe1;
  MockPipe pipe2;

  MockPipeByteStream remote(ctx, "Echo", loopback, loopback);

  MockPipeByteStream server_stream(ctx, "Server", pipe1, pipe2);
  ByteStreamTunnel server(ctx, server_stream);
  server.RegisterDriver(remote);

  MockPipeByteStream client_stream(ctx, "Client", pipe2, pipe1);
  ByteStreamTunnel client(ctx, client_stream);

  auto server_handshake = server.Handshake();
  auto client_handshake = client.Handshake();

  ASSERT_TRUE(server_handshake.done());
  ASSERT_TRUE(client_handshake.done());

  auto server_handshake_error = server_handshake.TakeResult();
  auto client_handshake_error = client_handshake.TakeResult();

  ASSERT_FALSE(server_handshake_error);
  ASSERT_FALSE(client_handshake_error);

  auto *forwarding = client.FindByName<ByteStream>("Echo");
  ASSERT_NE(nullptr, forwarding);

  {
    EXPECT_CALL(
        logger_output,
        CheckLine(LogLevel::ERROR, "[ERROR] [(Echo) tx] Corrupted action"));

    std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6};

    auto xact = forwarding->Initiate("tx");
    auto pwrite = xact.Add<WriteBytes>(std::as_bytes(std::span<uint8_t>(data)));

    auto *pwrite_action = reinterpret_cast<ByteStreamAction *>(
        reinterpret_cast<std::byte *>(pwrite.position.first.chunk) +
        pwrite.position.first.offset);

    const_cast<uint32_t &>(pwrite_action->As<WriteBytes>()->size) = 100;

    auto task = forwarding->Schedule(std::move(xact));
    ASSERT_TRUE(task.done());

    auto result = task.TakeResult();
    EXPECT_FALSE(result);
    EXPECT_EQ(result.Complete().begin(), result.Complete().end());
  }

  {
    EXPECT_CALL(
        logger_output,
        CheckLine(LogLevel::ERROR,
                  "[ERROR] [(Echo) tx] Unknown or corrupted action with ID 2"));

    auto xact = forwarding->Initiate("tx");
    auto pread = xact.Add<ReadBytes>(4);

    auto *pread_action = reinterpret_cast<ByteStreamAction *>(
        reinterpret_cast<std::byte *>(pread.position.first.chunk) +
        pread.position.first.offset);

    const_cast<uint32_t &>(pread_action->As<ReadBytes>()->size) = 100;

    auto task = forwarding->Schedule(std::move(xact));
    ASSERT_TRUE(task.done());

    auto result = task.TakeResult();
    EXPECT_FALSE(result);
    EXPECT_EQ(result.Complete().begin(), result.Complete().end());
  }
}
