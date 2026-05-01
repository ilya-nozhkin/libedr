#include "libedr/driver/execution_gate/ExecutionGate.h"
#include "libedr/driver/Error.hpp"
#include "libedr/driver/execution_gate/ExecutionGateAction.hpp"
#include <mutex>
#include <thread>

namespace edr {

void ExecutionGateImpl::Terminate() {
  {
    std::scoped_lock<std::mutex> lock(m_mutex);
    m_mode = ExecutionGateMode::Terminated;
  }

  m_on_new_request.notify_all();
}

void ExecutionGateImpl::Join(const std::coroutine_handle<> &to_complete) {
  while (!to_complete.done())
    std::this_thread::yield();
}

ExecutionGateMode ExecutionGateImpl::SetMode(ExecutionGateMode mode) {
  {
    std::scoped_lock<std::mutex> lock(m_mutex);

    if (m_mode == ExecutionGateMode::Terminated)
      return m_mode;

    m_mode = mode;
  }

  m_on_new_request.notify_all();
  return m_mode;
}

void ExecutionGateImpl::StallIfNeeded(bool target_is_idle) {
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

void ExecutionGateImpl::AddPending() {
  {
    std::scoped_lock<std::mutex> lock(m_mutex);
    m_num_requests++;
  }

  m_on_new_request.notify_all();
}

void ExecutionGateImpl::RemovePending() {
  std::scoped_lock<std::mutex> lock(m_mutex);
  m_num_requests--;
}

ExecutionGateImpl::CheckedTask<ExecutionGate::Status>
ExecutionGateImpl::Execute(TxInProgress &&tx) {
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
