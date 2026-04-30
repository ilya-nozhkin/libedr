#ifndef LIBEDR_API_RVDTM_H
#define LIBEDR_API_RVDTM_H

#include "Common.h"
#include "Context.h"
#include "JtagChain.h"

#include "libedr/driver/CommonActions.hpp"
#include "libedr/driver/Driver.hpp"
#include "libedr/driver/riscv/RVDTM.hpp"
#include "libedr/driver/riscv/RVDTMAction.hpp"
#include "libedr/driver/riscv/RVJtagDTM.h"
#include "libedr/util/miscellaneous/LatencyCalibrator.hpp"

class RVDTMTransaction {
  TRANSACTION_BODY(RVDTM);

public:
  void SkipCycles(uint32_t num_cycles) {
    if (!m_builder)
      return;

    m_builder->Add<edr::SkipCycles>(num_cycles);
  }

  void Write(uint32_t address, uint32_t data) {
    if (!m_builder)
      return;

    m_builder->Add<edr::RVWriteDMI>(address, data);
  }

  void Read(uint32_t address) {
    if (!m_builder)
      return;

    m_builder->Add<edr::RVReadDMI>(address);
  }

  uint32_t GetReadData() {
    if (ActionFail())
      return 0;

    auto *out = (*m_iterator)->Out<edr::RVReadDMI>();
    return nullptr == out ? 0 : out->data;
  }
};

class RVJtagDTMConfiguration final {
  friend class RVDTM;

public:
  RVJtagDTMConfiguration() = default;

  void SetTapID(unsigned tap_id) { m_config.tap_id = tap_id; }

  void SetDTMCSIRValue(uint32_t dtmcs_ir_value) {
    m_config.dtmcs_ir_value = dtmcs_ir_value;
  }

  void SetDMIIRValue(uint32_t dmi_ir_value) {
    m_config.dmi_ir_value = dmi_ir_value;
  }

  void SetMinMaxLatency(uint32_t min_latency, uint32_t max_latency) {
    m_config.latency = edr::LatencyCalibrator(min_latency, max_latency);
  }

private:
  edr::RVJtagDTMConfiguration m_config;
};

class RVDTM : public DriverBase {
  DRIVER_BODY(RVDTM);

  static RVDTM RVJtagDTM(const std::shared_ptr<Context> &context_sp,
                         const char *name, JtagChain &jtag_chain,
                         const RVJtagDTMConfiguration &config) {
    return RVDTM(context_sp, &context_sp->MakeWith<edr::RVJtagDTM>(
                                 context_sp->PersistFormat("{}", name),
                                 *jtag_chain.Self(), config.m_config));
  }
};

#endif
