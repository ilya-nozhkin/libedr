#ifndef LIBEDR_DRIVER_DRIVERIMPL_HPP
#define LIBEDR_DRIVER_DRIVERIMPL_HPP

#include "libedr/driver/Driver.hpp"

namespace edr {

template <DriverID t_driver_id, IsAction TAction>
TransactionStatus<TAction> Driver<t_driver_id, TAction>::MakeEmptyStatus() {
  DependentTransactions including_self{.prev = nullptr,
                                       .length = 0,
                                       .driver_name = m_name,
                                       .tx_description = ""};

  SpinlockGuard lock(m_spinlock);
  auto builder =
      Builder(TransactionStorage(m_spinlock, m_buffers,
                                 m_buffers.TakeOrMake(m_memory_resource),
                                 m_buffers.TakeOrMake(m_memory_resource)),
              including_self);

  return Status(std::move(builder));
}

template <DriverID t_driver_id, IsAction TAction>
TransactionBuilder<TAction>
Driver<t_driver_id, TAction>::Initiate(std::string_view tx_description,
                                       Status *reuse,
                                       const DependentTransactions *dependent) {
  DependentTransactions including_self{
      .prev = dependent,
      .length = nullptr == dependent ? 0 : 1 + dependent->length,
      .driver_name = m_name,
      .tx_description = tx_description};
  m_logger.Trace("{0:i}Initiating {0}", including_self);

  if (nullptr != reuse) {
    reuse->m_storage.ins.Clear();
    reuse->m_storage.outs.Clear();
    return Builder(std::move(reuse->m_storage), including_self);
  }

  SpinlockGuard lock(m_spinlock);
  return Builder(TransactionStorage(m_spinlock, m_buffers,
                                    m_buffers.TakeOrMake(m_memory_resource),
                                    m_buffers.TakeOrMake(m_memory_resource)),
                 including_self);
}

template <DriverID t_driver_id, IsAction TAction>
Driver<t_driver_id, TAction>::CheckedTask<TransactionStatus<TAction>>
Driver<t_driver_id, TAction>::Schedule(Builder &&builder) {
  auto dependent = builder.GetDependent();
  m_logger.Trace("{0:i}[{0}] Scheduling", dependent);

  if (builder.HadAllocationFailure()) {
    m_logger.Error("{0:i}[{0}] Invalid transaction input: Allocation failure",
                   dependent);
    return CheckedTask<Status>::MakeResolved(std::move(builder));
  }

  Status before(std::move(builder));
  if (before) {
    m_logger.Trace("{:i1}Empty transaction, returning immediately", dependent);
    return CheckedTask<Status>::MakeResolved(std::move(before));
  }

  TxInProgress tx(std::move(before));

  for (auto act : tx.Incomplete()) {
    if (!act) {
      std::optional<ActionID> action_id = act.GetID();

      if (action_id)
        m_logger.Error("{0:i}[{0}] Unknown or corrupted action with ID {1}",
                       dependent, *action_id);
      else
        m_logger.Error("{0:i}[{0}] Corrupted action", dependent);

      tx.template FailAt<CauseUnsupportedAction>(act);
      return CheckedTask<Status>::MakeResolved(tx.Finish());
    }

    act.Visit([this, &dependent](auto &action, auto &out) {
      m_logger.Trace("{:i1}{}", dependent, action);
    });
  }

  auto task = ScheduleAsync(dependent, std::move(tx));
  if (!task) {
    m_logger.Error("{0:i}[{0}] Failed to allocate a task frame, dropping",
                   dependent);
    return CheckedTask<Status>::MakeResolved(tx.Finish());
  }

  return task;
}

template <DriverID t_driver_id, IsAction TAction>
Driver<t_driver_id, TAction>::CheckedTask<TransactionStatus<TAction>>
Driver<t_driver_id, TAction>::ScheduleAsync(DependentTransactions dependent,
                                            TxInProgress &&tx) {
  TxInProgress tx_captured(std::move(tx));

  auto task = Execute(std::move(tx_captured));
  if (!task) {
    m_logger.Error("{0:i}[{0}] Failed to allocate a task frame, dropping",
                   dependent);
    co_return tx_captured.Finish();
  }

  auto after = co_await task;
  if (m_logger.GetLevel() < LogLevel::TRACE)
    co_return after;

  m_logger.Trace("{0:i}[{0}] Completed", dependent);
  for (auto act : after.Complete())
    act.Visit([this, &dependent](auto &action, auto &out) {
      m_logger.Trace("{:i1}{} -> {}", dependent, action, out);
    });

  if (!after) {
    ActionError *error = after.GetError();
    if (nullptr == error)
      m_logger.Trace("{:i1}Unknown error", dependent);
    else
      m_logger.Trace("{:i1}{}", dependent, *error);
  }

  co_return after;
}

} // namespace edr

#endif
