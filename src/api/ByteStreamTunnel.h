#ifndef LIBEDR_API_BYTESTREAMTUNNEL_H
#define LIBEDR_API_BYTESTREAMTUNNEL_H

#include "ByteStream.h"
#include "Context.h"
#include "TunnelCommon.h"

#include "api/DriverBase.h"
#include "libedr/tunnel/ByteStreamTunnel.h"

#include <memory>

class ByteStreamTunnel {
  TUNNEL_BODY;

public:
  explicit ByteStreamTunnel(const std::shared_ptr<Context> &context_sp,
                            ByteStream &byte_stream)
      : m_context_sp(context_sp),
        m_tunnel(m_context_sp->MakeWith<edr::ByteStreamTunnel>(
            *byte_stream.Self())) {}

  ~ByteStreamTunnel() {
    m_tunnel.Terminate();
    if (m_server.joinable())
      m_server.join();
  }

  void RegisterDriver(DriverBase &driver) {
    if (!driver.IsValid())
      return;

    m_tunnel.RegisterDriver(*driver.Base());
  }

  void Hanshake(Error &error) {
    auto task = m_tunnel.Handshake();
    m_tunnel.Join(task);

    if (!task->Success())
      error = std::move(*task);
  }

  void StartServer(Error &error) {
    Hanshake(error);
    if (error.Fail())
      return;

    m_server = std::thread([&tunnel = m_tunnel]() {
      while (tunnel.Serve(true))
        ;
    });
  }

  void Terminate() { return m_tunnel.Terminate(); }

  bool IsAlive() { return m_tunnel.IsAlive(); }

private:
  std::shared_ptr<Context> m_context_sp;
  edr::ByteStreamTunnel &m_tunnel;
  std::thread m_server;
};

#endif
