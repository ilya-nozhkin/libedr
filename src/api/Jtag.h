#ifndef LIBEDR_API_JTAG_H
#define LIBEDR_API_JTAG_H

#include "Common.h"
#include "Context.h"
#include "Error.h"

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/jtag/Jtag.hpp"
#include "libedr/driver/jtag/JtagAction.hpp"
#include "libedr/driver/jtag/PullJtag.h"
#include "libedr/util/adt/BitStream.hpp"

enum class JtagMode : uint32_t {
  StallIfNoRequestsAndIdle = 0, // Default
  ForceStallIfNoRequsts = 1,
  ForceStall = 2,
  NoStall = 3,
  Terminated = 4,
};

class JtagTransaction {
  TRANSACTION_BODY(Jtag, edr::Jtag);

public:
  void PutTMS(const std::byte *src_bits, size_t num_bits) {
    if (!m_builder)
      return;

    m_builder->Add<edr::PutTMS>(edr::BitStream<const uint8_t>(
        reinterpret_cast<const uint8_t *>(src_bits), num_bits));
  }

  void PutTDI(const std::byte *src_bits, size_t num_bits, uint32_t last_tms) {
    m_builder->Add<edr::PutTDI>(
        edr::BitStream<const uint8_t>(
            reinterpret_cast<const uint8_t *>(src_bits), num_bits),
        last_tms);
  }

  void PutTDIGetTDO(const std::byte *src_bits, size_t num_bits,
                    uint32_t last_tms) {
    m_builder->Add<edr::PutTDIGetTDO>(
        edr::BitStream<const uint8_t>(
            reinterpret_cast<const uint8_t *>(src_bits), num_bits),
        last_tms);
  }

  size_t GetNumBitsPut() {
    if (!InitCheckIterator())
      return 0;

    return (*m_iterator)
        ->Visit([]<class T>(const auto & /*action*/, const T &out) {
          return out.num_put;
        });
  }

  size_t GetTDO(std::byte *dest_bits, size_t max_num_bits) {
    if (!InitCheckIterator())
      return 0;

    auto *put_tdi_get_tdo_out = (*m_iterator)->Out<edr::PutTDIGetTDO>();
    if (nullptr == put_tdi_get_tdo_out)
      return 0;

    auto src_stream = put_tdi_get_tdo_out->Bits();
    edr::BitStream<uint8_t> dest_stream(reinterpret_cast<uint8_t *>(dest_bits),
                                        max_num_bits);

    return dest_stream.Write(src_stream, max_num_bits);
  }
};

class Jtag {
  DRIVER_BODY(Jtag, edr::Jtag);
};

class PullJtag final : public Jtag {
public:
  PullJtag(const std::shared_ptr<Context> &context_sp, const char *name)
      : Jtag(context_sp, &context_sp->MakeWith<edr::PullJtag>(
                             context_sp->PersistFormat("{}", name))) {}

  size_t PullTMSTDI(std::byte *tms_dest, size_t max_tms_bits,
                    std::byte *tdi_dest, size_t max_tdi_bits) {
    edr::BitStream<uint8_t> tms_stream(reinterpret_cast<uint8_t *>(tms_dest),
                                       max_tms_bits);
    edr::BitStream<uint8_t> tdi_stream(reinterpret_cast<uint8_t *>(tdi_dest),
                                       max_tdi_bits);
    return static_cast<edr::PullJtag &>(*m_driver).PullTMSTDI(tms_stream,
                                                              tdi_stream);
  }

  size_t PushTDO(const std::byte *src_bits, size_t num_bits) {
    edr::BitStream<const uint8_t> tdo_stream(
        reinterpret_cast<const uint8_t *>(src_bits), num_bits);
    return static_cast<edr::PullJtag &>(*m_driver).PushTDO(tdo_stream);
  }
};

#endif
