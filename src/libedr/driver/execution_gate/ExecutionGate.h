#ifndef LIBEDR_DRIVER_EXECUTION_GATE_EXECUTIONGATE_HPP
#define LIBEDR_DRIVER_EXECUTION_GATE_EXECUTIONGATE_HPP

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/driver/execution_gate/ExecutionGateAction.hpp"

#include <condition_variable>

namespace edr {

class ExecutionGate final
    : public Driver<DriverID::ExecutionGate, ExecutionGateAction> {
public:
  ExecutionGate(const DriverContext &ctx, std::string_view name);

  ~ExecutionGate() { Terminate(); }

  void Terminate() override;

  void Join(const std::coroutine_handle<> &to_complete) override;

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
