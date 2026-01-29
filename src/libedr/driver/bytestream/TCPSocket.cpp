#include "libedr/driver/bytestream/TCPSocket.h"
#include "libedr/driver/Error.hpp"
#include "libedr/util/miscellaneous/ScopeGuard.hpp"

#include <format>
#include <memory_resource>
#include <string>

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace edr {

std::expected<TCPSocket, Error> TCPSocket::Connect(const DriverContext &context,
                                                   std::string_view hostname,
                                                   uint16_t port) {
  std::pmr::string hostname_zero_terminated(
      hostname,
      std::pmr::polymorphic_allocator<>(&context.TaskFrameResource()));

  struct addrinfo hints{};
  struct addrinfo *addrinfos = nullptr;

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  char port_string[16] = {};
  std::format_to_n(port_string, sizeof(port_string), "{}", port);

  auto getaddrinfo_result = getaddrinfo(hostname_zero_terminated.c_str(),
                                        port_string, &hints, &addrinfos);
  if (0 != getaddrinfo_result)
    return std::unexpected(
        context.MakeError<CauseErrno>("getaddrinfo", getaddrinfo_result));

  if (nullptr == addrinfos)
    return std::unexpected(context.MakeErrorCauseStringMessage(
        "No addresses found by host name '{}'", hostname_zero_terminated));

  auto addrinfo_guard =
      MakeScopeGuard([addrinfos]() { freeaddrinfo(addrinfos); });

  auto client_socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (g_invalid_socket == client_socket_fd)
    return std::unexpected(
        context.MakeError<CauseErrno>("socket", client_socket_fd));

  auto socket_guard =
      MakeScopeGuard([client_socket_fd] { close(client_socket_fd); });

  auto connect_result =
      connect(client_socket_fd, addrinfos->ai_addr, addrinfos->ai_addrlen);
  if (0 != connect_result)
    return std::unexpected(
        context.MakeError<CauseErrno>("connect", connect_result));

  int flag = 1;
  auto setsockopt_result = setsockopt(client_socket_fd, IPPROTO_TCP,
                                      TCP_NODELAY, &flag, sizeof(flag));
  if (0 != setsockopt_result)
    return std::unexpected(context.MakeError<CauseErrno>(
        "setsockopt(..., IPPROTO_TCP, TCP_NODELAY, 1, ...)",
        setsockopt_result));

  socket_guard.Release();
  return TCPSocket(context, client_socket_fd);
}

std::pair<size_t, Error> TCPSocket::Write(std::span<const std::byte> source) {
  auto written = send(m_socket, source.data(), source.size(), 0);
  if (written > 0)
    return {written, Error::Success()};

  return {0, m_context.MakeError<CauseErrno>("send", written)};
}

std::pair<size_t, Error> TCPSocket::Read(std::span<std::byte> dest) {
  auto received = recv(m_socket, dest.data(), dest.size(), 0);
  if (received > 0)
    return {received, Error::Success()};

  if (received == 0)
    return {0, m_context.MakeError<CauseTerminated>("TCP socket")};

  return {0, m_context.MakeError<CauseErrno>("recv", received)};
}

void TCPSocket::Terminate() {
  if (g_invalid_socket != m_socket) {
    close(m_socket);
    m_socket = g_invalid_socket;
  }
}

std::expected<TCPServer, Error> TCPServer::Create(const DriverContext &context,
                                                  uint16_t port,
                                                  int max_num_queued_clients) {
  auto &error_resource = context.TransactionBufferResource();

  auto server_socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (g_invalid_socket == server_socket_fd)
    return std::unexpected(
        context.MakeError<CauseErrno>("socket", server_socket_fd));

  auto socket_guard =
      MakeScopeGuard([server_socket_fd] { close(server_socket_fd); });

  {
    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);

    auto bind_result =
        bind(server_socket_fd, reinterpret_cast<sockaddr *>(&server_address),
             sizeof(server_address));
    if (0 != bind_result)
      return std::unexpected(
          context.MakeError<CauseErrno>("bind", bind_result));

    auto listen_result = listen(server_socket_fd, max_num_queued_clients);
    if (0 != listen_result)
      return std::unexpected(
          context.MakeError<CauseErrno>("listen", listen_result));
  }

  sockaddr_in resulting_address{};

  socklen_t addr_len = sizeof(resulting_address);
  auto getsockname_result =
      getsockname(server_socket_fd,
                  reinterpret_cast<sockaddr *>(&resulting_address), &addr_len);
  if (0 != getsockname_result)
    return std::unexpected(
        context.MakeError<CauseErrno>("getsockname", getsockname_result));

  socket_guard.Release();
  return TCPServer(context, server_socket_fd,
                   ntohs(resulting_address.sin_port));
}

TCPServer::~TCPServer() {
  if (g_invalid_socket != m_server_socket)
    close(m_server_socket);
}

std::expected<TCPSocket, Error> TCPServer::Accept() {
  sockaddr_in client_address{};
  socklen_t client_address_length = sizeof(client_address);

  auto client_socket_fd =
      accept(m_server_socket, reinterpret_cast<sockaddr *>(&client_address),
             &client_address_length);
  if (g_invalid_socket == client_socket_fd)
    return std::unexpected(
        m_context.MakeError<CauseErrno>("accept", client_socket_fd));

  int flag = 1;
  auto setsockopt_result = setsockopt(client_socket_fd, IPPROTO_TCP,
                                      TCP_NODELAY, &flag, sizeof(flag));
  if (0 != setsockopt_result)
    return std::unexpected(m_context.MakeError<CauseErrno>(
        "setsockopt(..., IPPROTO_TCP, TCP_NODELAY, 1, ...)",
        setsockopt_result));

  return TCPSocket(m_context, client_socket_fd);
}

} // namespace edr
