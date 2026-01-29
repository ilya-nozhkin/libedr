#ifndef LIBEDR_DRIVER_BYTESTREAM_TCPSOCKET_H
#define LIBEDR_DRIVER_BYTESTREAM_TCPSOCKET_H

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/bytestream/ByteStream.hpp"

#include <cstdint>
#include <expected>

namespace edr {

using SocketType = int;
inline constexpr SocketType g_invalid_socket = -1;

class TCPSocket final : public BlockingByteStream {
  friend class TCPServer;

public:
  static std::expected<TCPSocket, Error> Connect(const DriverContext &context,
                                                 std::string_view hostname,
                                                 uint16_t port);

  TCPSocket(TCPSocket &&from)
      : m_context(from.m_context), m_socket(from.m_socket) {
    from.m_socket = g_invalid_socket;
  }

  TCPSocket(const TCPSocket &) = delete;
  TCPSocket &operator=(const TCPSocket &) = delete;
  TCPSocket &operator=(TCPSocket &&) = delete;

  ~TCPSocket() override { Terminate(); }

  std::pair<size_t, Error> Write(std::span<const std::byte> source) override;

  std::pair<size_t, Error> Read(std::span<std::byte> dest) override;

  void Terminate() override;

private:
  explicit TCPSocket(const DriverContext &context, SocketType socket)
      : m_context(context), m_socket(socket) {}

  DriverContext m_context;
  SocketType m_socket;
};

class TCPServer final {
public:
  static std::expected<TCPServer, Error> Create(const DriverContext &context,
                                                uint16_t port,
                                                int max_num_queued_clients = 1);

  TCPServer(TCPServer &&from)
      : m_context(from.m_context), m_server_socket(from.m_server_socket),
        m_port(from.m_port) {
    from.m_server_socket = g_invalid_socket;
  }

  TCPServer(const TCPServer &) = delete;
  TCPServer &operator=(const TCPServer &) = delete;
  TCPServer &operator=(TCPServer &&) = delete;

  ~TCPServer();

  uint16_t GetPort() const { return m_port; }

  std::expected<TCPSocket, Error> Accept();

private:
  TCPServer(const DriverContext &context, SocketType server_socket,
            uint16_t port)
      : m_context(context), m_server_socket(server_socket), m_port(port) {}

  DriverContext m_context;
  SocketType m_server_socket;
  uint16_t m_port;
};

} // namespace edr

#endif
