#ifndef LIBEDR_DRIVER_EXECUTION_GATE_EXECUTIONGATE_HPP
#define LIBEDR_DRIVER_EXECUTION_GATE_EXECUTIONGATE_HPP

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/driver/execution_gate/ExecutionGateAction.hpp"

#include <condition_variable>

namespace edr {

using ExecutionGate = Driver<DriverID::ExecutionGate, ExecutionGateAction>;

class ExecutionGateImpl : public ExecutionGate {
public:
  using ExecutionGate::ExecutionGate;

  ~ExecutionGateImpl() { Terminate(); }

  void Terminate() override;

  bool Serve(bool /*wait_if_empty*/) override { return false; }

  void Join(const std::coroutine_handle<> &to_complete) override;

  ExecutionGateMode SetMode(ExecutionGateMode mode);

  void StallIfNeeded(bool target_is_idle);

  void AddPending();

  void RemovePending();

private:
  CheckedTask<Status> Execute(TxInProgress &&tx) override;

  std::mutex m_mutex;
  std::condition_variable m_on_new_request;
  ExecutionGateMode m_mode = ExecutionGateMode::StallIfNoRequestsAndIdle;
  size_t m_num_requests = 0;
};

} // namespace edr

#endif
