#ifndef DRIVER_BYTESTREAM_MOCKS_H
#define DRIVER_BYTESTREAM_MOCKS_H

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/bytestream/ByteStream.hpp"
#include "libedr/util/asynchronicity/AsynchronousPrimitives.hpp"

#include <coroutine>
#include <cstddef>
#include <vector>

namespace edr {

class MockPipe : public Asynchronous<> {
public:
  MockPipe() : m_queue(*this) {}

  void Write(std::span<const std::byte> bytes) {
    m_data.insert(m_data.end(), bytes.begin(), bytes.end());

    while (!m_queue.Empty() && m_data.size() >= m_queue.Front())
      m_queue.Pop().Resolve();
  }

  Task<> Read(std::span<std::byte> dest) {
    if (m_data.size() < dest.size()) {
      auto awaitable = m_queue.Emplace(dest.size());
      assert(awaitable);
      co_await awaitable;
    }

    memcpy(dest.data(), m_data.data(), dest.size());
    m_data.erase(m_data.begin(), m_data.begin() + dest.size());
  }

private:
  std::vector<std::byte> m_data;
  ResolutionQueue<MockPipe, size_t> m_queue;
};

class MockPipeByteStream final : public ByteStream {
public:
  MockPipeByteStream(const DriverContext &ctx, std::string_view name,
                     MockPipe &write_pipe, MockPipe &read_pipe)
      : Driver(ctx, name), m_write_pipe(write_pipe), m_read_pipe(read_pipe) {}

  void FailAfter(size_t num_actions) { m_actions_left = num_actions; }

  void Join(const std::coroutine_handle<> &to_complete) override {}

  void Terminate() override {}

private:
  CheckedTask<Status> Execute(TxInProgress &&tx) override {
    for (auto act : tx.Incomplete()) {
      auto [write, write_out] = act.As<WriteBytes>();
      auto [read, read_out] = act.As<ReadBytes>();

      if (m_actions_left.has_value()) {
        if (*m_actions_left == 0) {
          tx.FailCauseStringMessage("Injected error");
          m_actions_left.reset();
          co_return tx.Finish();
        }

        (*m_actions_left)--;
      }

      if (nullptr != write) {
        m_write_pipe.Write(write->Span());
        write_out->num_written = write->size;
      }

      if (nullptr != read) {
        co_await m_read_pipe.Read(std::span<std::byte>(
            reinterpret_cast<std::byte *>(read_out->Data()), read->size));
        read_out->num_read = read->size;
      }

      tx.Done(act);
    }

    co_return tx.Finish();
  }

  MockPipe &m_write_pipe;
  MockPipe &m_read_pipe;

  std::vector<std::byte> m_data;
  std::optional<size_t> m_actions_left;
};

} // namespace edr

#endif
