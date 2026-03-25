#ifndef LIBEDR_API_BYTESTREAM_H
#define LIBEDR_API_BYTESTREAM_H

#include "Common.h"
#include "Context.h"
#include "DriverBase.h"
#include "Error.h"

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/bytestream/ByteStream.hpp"
#include "libedr/driver/bytestream/ByteStreamAction.hpp"
#include "libedr/driver/bytestream/NamedPipe.h"
#include "libedr/driver/bytestream/TCPSocket.h"

#include <memory>

class ByteStreamTransaction {
  TRANSACTION_BODY(ByteStream);

public:
  void WriteBytes(const std::byte *src, uint32_t size) {
    if (!m_builder)
      return;

    m_builder->Add<edr::WriteBytes>(std::span<const std::byte>(src, size));
  }

  void ReadBytes(uint32_t num_bytes) {
    if (!m_builder)
      return;

    m_builder->Add<edr::ReadBytes>(num_bytes);
  }

  uint32_t GetNumWrittenBytes() {
    if (ActionFail())
      return 0;

    auto [_, out] = (*m_iterator)->As<edr::WriteBytes>();
    if (nullptr == out)
      return 0;

    return out->num_written;
  }

  uint32_t GetReadBytes(std::byte *dest, uint32_t size) {
    if (ActionFail())
      return 0;

    auto [_, out] = (*m_iterator)->As<edr::ReadBytes>();
    if (nullptr == out)
      return 0;

    auto span = out->Span();
    auto to_copy = std::min<uint32_t>(size, span.size());
    memcpy(dest, span.data(), to_copy);

    return to_copy;
  }
};

class ByteStream : public DriverBase {
  DRIVER_BODY(ByteStream);

public:
  static ByteStream ConnectTCP(const std::shared_ptr<Context> &context_sp,
                               const char *hostname, uint16_t port,
                               Error &error) {
    if (nullptr == hostname)
      return ByteStream(context_sp, nullptr);

    auto *persistent_socket =
        context_sp->CallWith(error, edr::TCPSocket::Connect, hostname, port);
    if (nullptr == persistent_socket)
      return ByteStream(context_sp, nullptr);

    edr::ByteStream &bstream = context_sp->MakeWith<edr::DeferredByteStream>(
        context_sp->PersistFormat("TCP -> {}:{}", hostname, port),
        *persistent_socket);

    return ByteStream(context_sp, &bstream);
  }

  static ByteStream ConnectNamedPipe(const std::shared_ptr<Context> &context_sp,
                                     const char *pipe_name, Error &error) {
    if (nullptr == pipe_name)
      return ByteStream(context_sp, nullptr);

    auto *persistent_pipe =
        context_sp->CallWith(error, edr::NamedPipe::Connect, pipe_name);
    if (nullptr == persistent_pipe)
      return ByteStream(context_sp, nullptr);

    edr::ByteStream &bstream = context_sp->MakeWith<edr::DeferredByteStream>(
        context_sp->PersistFormat("NamedPipe -> {}", pipe_name),
        *persistent_pipe);

    return ByteStream(context_sp, &bstream);
  }
};

class TCPServer {
public:
  explicit TCPServer(const std::shared_ptr<Context> &context_sp, uint16_t port,
                     int max_num_queued_clients, Error &error)
      : m_context_sp(context_sp),
        m_server(m_context_sp->CallWith(error, edr::TCPServer::Create, port,
                                        max_num_queued_clients)) {}

  bool IsValid() const { return nullptr != m_server; }

  ByteStream Accept(Error &error) {
    if (nullptr == m_server)
      return ByteStream(m_context_sp, nullptr);

    auto exp_socket = m_server->Accept();
    if (!exp_socket) {
      error = std::move(exp_socket.error());
      return ByteStream(m_context_sp, nullptr);
    }

    edr::TCPSocket &persistent_socket =
        m_context_sp->Store(std::move(*exp_socket));

    edr::ByteStream &bstream = m_context_sp->MakeWith<edr::DeferredByteStream>(
        m_context_sp->PersistFormat("TCP <- {}", m_server->GetPort()),
        persistent_socket);

    return ByteStream(m_context_sp, &bstream);
  }

  uint16_t GetPort() {
    if (nullptr == m_server)
      return 0;

    return m_server->GetPort();
  }

private:
  std::shared_ptr<Context> m_context_sp;
  edr::TCPServer *m_server = nullptr;
};

class NamedPipeServer {
public:
  explicit NamedPipeServer(const std::shared_ptr<Context> &context_sp,
                           const char *pipe_name, int max_num_queued_clients,
                           Error &error)
      : m_context_sp(context_sp),
        m_server(m_context_sp->CallWith(error, edr::NamedPipeServer::Create,
                                        pipe_name, max_num_queued_clients)),
        m_persistent_name(m_context_sp->PersistFormat("{}", pipe_name)) {}

  bool IsValid() const { return nullptr != m_server; }

  ByteStream Accept(Error &error) {
    if (nullptr == m_server)
      return ByteStream(m_context_sp, nullptr);

    auto exp_pipe = m_server->Accept();
    if (!exp_pipe) {
      error = std::move(exp_pipe.error());
      return ByteStream(m_context_sp, nullptr);
    }

    edr::NamedPipe &persistent_pipe = m_context_sp->Store(std::move(*exp_pipe));

    edr::ByteStream &bstream = m_context_sp->MakeWith<edr::DeferredByteStream>(
        m_context_sp->PersistFormat("NamedPipe <- {}", m_persistent_name),
        persistent_pipe);

    return ByteStream(m_context_sp, &bstream);
  }

private:
  std::shared_ptr<Context> m_context_sp;
  edr::NamedPipeServer *m_server = nullptr;
  const char *m_persistent_name = nullptr;
};

#endif
