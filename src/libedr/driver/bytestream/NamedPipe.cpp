#include "libedr/driver/bytestream/NamedPipe.h"
#include "libedr/driver/Error.hpp"
#include "libedr/util/miscellaneous/ScopeGuard.hpp"

#include <format>

#include <iterator>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

namespace edr {

std::expected<NamedPipe, Error> NamedPipe::Connect(const DriverContext &context,
                                                   std::string_view pipe_name) {
  auto client_named_pipe_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (g_invalid_named_pipe == client_named_pipe_fd)
    return std::unexpected(
        context.MakeError<CauseErrno>("socket", client_named_pipe_fd));

  auto named_pipe_guard =
      MakeScopeGuard([client_named_pipe_fd] { close(client_named_pipe_fd); });

  sockaddr_un client_address{};
  client_address.sun_family = AF_UNIX;
  auto format_result =
      std::format_to_n(client_address.sun_path, sizeof(client_address.sun_path),
                       "/tmp/{}", pipe_name);

  if (format_result.size >= sizeof(client_address.sun_path))
    return std::unexpected(
        context.MakeErrorCauseStringMessage("The pipe name is too long"));

  auto connect_result = connect(client_named_pipe_fd,
                                reinterpret_cast<sockaddr *>(&client_address),
                                sizeof(client_address));
  if (0 != connect_result)
    return std::unexpected(
        context.MakeError<CauseErrno>("connect", connect_result));

  named_pipe_guard.Release();
  return NamedPipe(context, client_named_pipe_fd);
}

std::pair<size_t, Error> NamedPipe::Write(std::span<const std::byte> source) {
  auto written = send(m_pipe, source.data(), source.size(), 0);
  if (written > 0)
    return {written, Error::Success()};

  return {0, m_context.MakeError<CauseErrno>("send", written)};
}

std::pair<size_t, Error> NamedPipe::Read(std::span<std::byte> dest) {
  auto received = recv(m_pipe, dest.data(), dest.size(), 0);
  if (received > 0)
    return {received, Error::Success()};

  if (received == 0)
    return {0, m_context.MakeError<CauseTerminated>("Named pipe")};

  return {0, m_context.MakeError<CauseErrno>("recv", received)};
}

void NamedPipe::Terminate() {
  if (g_invalid_named_pipe != m_pipe) {
    close(m_pipe);
    m_pipe = g_invalid_named_pipe;
  }
}

std::expected<NamedPipeServer, Error>
NamedPipeServer::Create(const DriverContext &context,
                        std::string_view pipe_name,
                        int max_num_queued_clients) {
  auto pipe_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (g_invalid_named_pipe == pipe_server_fd)
    return std::unexpected(
        context.MakeError<CauseErrno>("socket", pipe_server_fd));

  auto named_pipe_guard =
      MakeScopeGuard([pipe_server_fd] { close(pipe_server_fd); });

  sockaddr_un server_address{};
  server_address.sun_family = AF_UNIX;
  auto format_result =
      std::format_to_n(server_address.sun_path, sizeof(server_address.sun_path),
                       "/tmp/{}", pipe_name);

  if (format_result.size >= sizeof(server_address.sun_path))
    return std::unexpected(
        context.MakeErrorCauseStringMessage("The pipe name is too long"));

  auto bind_result =
      bind(pipe_server_fd, reinterpret_cast<sockaddr *>(&server_address),
           sizeof(server_address));
  if (0 != bind_result)
    return std::unexpected(context.MakeError<CauseErrno>("bind", bind_result));

  auto listen_result = listen(pipe_server_fd, max_num_queued_clients);
  if (0 != listen_result)
    return std::unexpected(
        context.MakeError<CauseErrno>("listen", listen_result));

  named_pipe_guard.Release();
  return NamedPipeServer(context, pipe_server_fd);
}

NamedPipeServer::~NamedPipeServer() {
  if (g_invalid_named_pipe != m_pipe_server)
    close(m_pipe_server);
}

std::expected<NamedPipe, Error> NamedPipeServer::Accept() {
  sockaddr_un client_address{};
  socklen_t client_address_length = sizeof(client_address);

  auto client_named_pipe_fd =
      accept(m_pipe_server, reinterpret_cast<sockaddr *>(&client_address),
             &client_address_length);
  if (g_invalid_named_pipe == client_named_pipe_fd)
    return std::unexpected(
        m_context.MakeError<CauseErrno>("accept", client_named_pipe_fd));

  return NamedPipe(m_context, client_named_pipe_fd);
}

} // namespace edr
