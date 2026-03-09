#ifndef LIBEDR_DRIVER_BYTESTREAM_NAMEDPIPE_H
#define LIBEDR_DRIVER_BYTESTREAM_NAMEDPIPE_H

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/bytestream/ByteStream.hpp"

#include <expected>

namespace edr {

using NamedPipeType = int;
inline constexpr NamedPipeType g_invalid_named_pipe = -1;

class NamedPipe final : public BlockingByteStream {
  friend class NamedPipeServer;

public:
  static std::expected<NamedPipe, Error> Connect(const DriverContext &context,
                                                 std::string_view pipe_name);

  NamedPipe(NamedPipe &&from) : m_context(from.m_context), m_pipe(from.m_pipe) {
    from.m_pipe = g_invalid_named_pipe;
  }

  NamedPipe(const NamedPipe &) = delete;
  NamedPipe &operator=(const NamedPipe &) = delete;
  NamedPipe &operator=(NamedPipe &&) = delete;

  ~NamedPipe() override { Terminate(); }

  std::pair<size_t, Error> Write(std::span<const std::byte> source) override;

  std::pair<size_t, Error> Read(std::span<std::byte> dest) override;

  void Terminate() override;

private:
  explicit NamedPipe(const DriverContext &context, NamedPipeType pipe)
      : m_context(context), m_pipe(pipe) {}

  DriverContext m_context;
  NamedPipeType m_pipe;
};

class NamedPipeServer final {
public:
  static std::expected<NamedPipeServer, Error>
  Create(const DriverContext &context, std::string_view pipe_name,
         int max_num_queued_clients = 1);

  NamedPipeServer(NamedPipeServer &&from)
      : m_context(from.m_context), m_pipe_server(from.m_pipe_server) {
    from.m_pipe_server = g_invalid_named_pipe;
  }

  NamedPipeServer(const NamedPipeServer &) = delete;
  NamedPipeServer &operator=(const NamedPipeServer &) = delete;
  NamedPipeServer &operator=(NamedPipeServer &&) = delete;

  ~NamedPipeServer();

  std::expected<NamedPipe, Error> Accept();

private:
  NamedPipeServer(const DriverContext &context, int pipe_server)
      : m_context(context), m_pipe_server(pipe_server) {}

  DriverContext m_context;
  int m_pipe_server;
};

} // namespace edr

#endif
