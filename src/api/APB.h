#ifndef LIBEDR_API_APB_H
#define LIBEDR_API_APB_H

#include "Common.h"
#include "Context.h"

#include "api/ExecutionGate.h"
#include "libedr/driver/CommonActions.hpp"
#include "libedr/driver/Driver.hpp"
#include "libedr/driver/apb/APB.hpp"
#include "libedr/driver/apb/APBAction.hpp"
#include "libedr/driver/apb/PullAPB.h"

class APBTransaction {
  TRANSACTION_BODY(APB);

public:
  void SkipCycles(uint32_t num_cycles) {
    if (!m_builder)
      return;

    m_builder->Add<edr::SkipCycles>(num_cycles);
  }

  void Write(uint32_t address, uint32_t data) {
    if (!m_builder)
      return;

    m_builder->Add<edr::APBWrite>(address, data);
  }

  void Read(uint32_t address) {
    if (!m_builder)
      return;

    m_builder->Add<edr::APBRead>(address);
  }

  void SetPSEL(uint32_t psel) {
    if (!m_builder)
      return;

    m_builder->Add<edr::APBSetPSEL>(psel);
  }

  uint32_t GetReadData() {
    if (ActionFail())
      return 0;

    auto *out = (*m_iterator)->Out<edr::APBRead>();
    return nullptr == out ? 0 : out->data;
  }
};

class APB : public DriverBase {
  DRIVER_BODY(APB);
};

class PullAPB final : public APB {
public:
  ~PullAPB() override = default;

  PullAPB(const std::shared_ptr<Context> &context_sp, const char *name,
          ExecutionGate &exe_gate)
      : APB(context_sp,
            &context_sp->MakeWith<edr::PullAPB>(
                context_sp->PersistFormat("{}", name),
                static_cast<edr::ExecutionGateImpl *>(exe_gate.Self()))) {}

  uint32_t PullCommands(uint8_t *commands_dst, uint32_t *addr_dst,
                        uint32_t *data_dst, uint32_t max_num_commands) {
    return static_cast<edr::PullAPB &>(*m_driver).PullCommands(
        reinterpret_cast<edr::PullAPB::PulledCommand *>(commands_dst), addr_dst,
        data_dst, max_num_commands);
  }

  uint32_t PushResults(const uint8_t *status_src, const uint32_t *data_src,
                       unsigned num_results) {
    return static_cast<edr::PullAPB &>(*m_driver).PushResults(
        reinterpret_cast<const edr::PullAPB::PushedStatus *>(status_src),
        data_src, num_results);
  }
};

#endif
