#include "libedr/driver/execution_gate/ExecutionGate.h"
#include "libedr/driver/Error.hpp"
#include "libedr/driver/execution_gate/ExecutionGateAction.hpp"
#include <mutex>
#include <thread>

namespace edr {

ExecutionGate::ExecutionGate(const DriverContext &ctx, std::string_view name)
    : Driver(ctx, name) {}

void ExecutionGate::Terminate() {
  {
    std::scoped_lock<std::mutex> lock(m_mutex);
    m_mode = ExecutionGateMode::Terminated;
  }

  m_on_new_request.notify_all();
}

void ExecutionGate::Join(const std::coroutine_handle<> &to_complete) {
  while (!to_complete.done())
    std::this_thread::yield();
}

void ExecutionGate::StallIfNeeded(bool target_is_idle) {
  std::unique_lock<std::mutex> lock(m_mutex);

  do {
    if (m_mode == ExecutionGateMode::Terminated ||
        m_mode == ExecutionGateMode::NoStall)
      return;

    if (m_mode == ExecutionGateMode::ForceStall ||
        (m_num_requests == 0 &&
         (m_mode == ExecutionGateMode::ForceStallIfNoRequsts ||
          (m_mode == ExecutionGateMode::StallIfNoRequestsAndIdle &&
           target_is_idle)))) {
      m_on_new_request.wait(lock);
      continue;
    }

    return;
  } while (true);
}

void ExecutionGate::AddPending() {
  {
    std::scoped_lock<std::mutex> lock(m_mutex);
    m_num_requests++;
  }

  m_on_new_request.notify_all();
}

void ExecutionGate::RemovePending() {
  std::scoped_lock<std::mutex> lock(m_mutex);
  m_num_requests--;
}

ExecutionGate::CheckedTask<ExecutionGate::Status>
ExecutionGate::Execute(TxInProgress &&tx) {
  for (auto action : tx.Incomplete()) {
    auto [set_mode, set_mode_out] = action.As<SetExecutionGateMode>();
    assert(nullptr != set_mode);

    {
      std::scoped_lock<std::mutex> lock(m_mutex);

      set_mode_out->effective_mode = m_mode;

      if (m_mode == ExecutionGateMode::Terminated) {
        tx.Fail<CauseTerminated>(m_name);
        co_return tx.Finish();
      }

      if (static_cast<uint32_t>(set_mode->mode) >=
          static_cast<uint32_t>(ExecutionGateMode::Terminated)) {
        tx.Fail<CauseInvalidArgument>("mode");
        co_return tx.Finish();
      }

      m_mode = set_mode->mode;
      set_mode_out->effective_mode = m_mode;
    }

    m_on_new_request.notify_all();

    tx.Done(action);
  }

  co_return tx.Finish();
}

} // namespace edr
