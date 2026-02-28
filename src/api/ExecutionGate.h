#ifndef LIBEDR_API_EXECUTIONGATE_H
#define LIBEDR_API_EXECUTIONGATE_H

#include "Common.h"
#include "Context.h"
#include "Error.h"

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/execution_gate/ExecutionGate.h"
#include "libedr/driver/execution_gate/ExecutionGateAction.hpp"

enum class ExecutionGateMode : uint32_t {
  StallIfNoRequestsAndIdle = 0, // Default
  ForceStallIfNoRequsts = 1,
  ForceStall = 2,
  NoStall = 3,
  Terminated = 4,
};

class ExecutionGateTransaction {
  TRANSACTION_BODY(ExecutionGate, edr::ExecutionGate);

public:
  void SetMode(ExecutionGateMode mode) {
    if (!m_builder)
      return;

    m_builder->Add<edr::SetExecutionGateMode>(
        static_cast<edr::ExecutionGateMode>(mode));
  }

  ExecutionGateMode GetEffectiveMode() {
    if (!InitCheckIterator())
      return ExecutionGateMode::StallIfNoRequestsAndIdle;

    auto [_, out] = (*m_iterator)->As<edr::SetExecutionGateMode>();
    if (nullptr == out)
      return ExecutionGateMode::StallIfNoRequestsAndIdle;

    return static_cast<ExecutionGateMode>(out->effective_mode);
  }
};

class ExecutionGate {
  DRIVER_BODY(ExecutionGate, edr::ExecutionGate);

public:
  ExecutionGate(const std::shared_ptr<Context> &context_sp, const char *name)
      : ExecutionGate(context_sp, &context_sp->MakeWith<edr::ExecutionGate>(
                                      context_sp->PersistFormat("{}", name))) {}
};

#endif
