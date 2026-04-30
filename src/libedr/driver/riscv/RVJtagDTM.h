#ifndef LIBEDR_DRIVER_RISCV_RVJTAGDTM_HPP
#define LIBEDR_DRIVER_RISCV_RVJTAGDTM_HPP

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/jtag/JtagChain.h"
#include "libedr/driver/riscv/RVDTM.hpp"
#include "libedr/util/asynchronicity/AsyncGate.hpp"
#include "libedr/util/miscellaneous/LatencyCalibrator.hpp"

namespace edr {

struct RVJtagDTMConfiguration {
  unsigned tap_id = 0;
  uint32_t dtmcs_ir_value = 0x10;
  uint32_t dmi_ir_value = 0x11;
  LatencyCalibrator latency = LatencyCalibrator(0, 1000);
};

class RVJtagDTM : public RVDTM {
public:
  RVJtagDTM(const DriverContext &ctx, std::string_view name, JtagChain &jtag,
            const RVJtagDTMConfiguration &config)
      : Driver(ctx, name), m_jtag(jtag), m_config(config) {}

  ~RVJtagDTM() override = default;

  void Terminate() override { m_jtag.Terminate(); }

  bool Serve(bool wait_if_empty) override {
    return m_jtag.Serve(wait_if_empty);
  }

  void Join(const std::coroutine_handle<> &to_complete) override {
    m_jtag.Join(to_complete);
  }

private:
  enum class DTMState {
    Initial,
    Operating,
    Error,
    Invalid,
  };

  enum class AttemptStatus {
    Success,
    MyFailure,
    PrevFailure,
    MyBusy,
    PrevBusy,
  };

  struct State {
    DTMState dtm_state = DTMState::Initial;
    size_t num_errors = 0;
  };

  CheckedTask<Status> Execute(TxInProgress &&tx) override;

  Task<bool> Discover(std::unique_lock<std::mutex> &lock, TxInProgress &tx);

  Task<AttemptStatus> Attempt(std::unique_lock<std::mutex> &lock,
                              TxInProgress &tx, AttemptStatus prev_attempt);

  JtagChain &m_jtag;
  RVJtagDTMConfiguration m_config;

  std::mutex m_mutex;
  State m_state;
  AsyncGate m_discovery_gate;

  uint32_t m_dmi_size = 0;
};

} // namespace edr

#endif
