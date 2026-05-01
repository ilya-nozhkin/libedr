#include "libedr/driver/apb/PullAPB.h"
#include "libedr/driver/CommonActions.hpp"
#include "libedr/driver/Error.hpp"
#include "libedr/driver/apb/APBAction.hpp"
#include "libedr/util/miscellaneous/Overload.hpp"

#include <mutex>

namespace edr {

PullAPB::PullAPB(const DriverContext &ctx, std::string_view name,
                 ExecutionGateImpl *exe_gate)
    : APB(ctx, name), m_exe_gate(exe_gate),
      m_command_generator(GenerateCommands()),
      m_result_consumer(ConsumeResults()) {}

void PullAPB::Terminate() {
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    while (!m_queue.NoScheduled())
      m_queue.StartNextScheduled();

    while (!m_queue.NoInProgress()) {
      auto item = m_queue.PrepareToResolve();

      lock.unlock();
      item.Resolve();
      lock.lock();
    }
  }

  if (nullptr != m_exe_gate)
    m_exe_gate->Terminate();
}

void PullAPB::Join(const std::coroutine_handle<> &to_complete) {}

size_t PullAPB::PullCommands(PulledCommand *commands_dst, uint32_t *addr_dst,
                             uint32_t *data_dst, size_t max_num_commands) {
  return m_command_generator(commands_dst, addr_dst, data_dst,
                             max_num_commands);
}

size_t PullAPB::PushResults(const PushedStatus *status_src,
                            const uint32_t *data_src, size_t num_results) {
  return m_result_consumer(status_src, data_src, num_results);
}

PullAPB::CheckedTask<PullAPB::Status> PullAPB::Execute(TxInProgress &&tx) {
  TransactionQueue::Item item(tx);

  std::unique_lock<std::mutex> lock(m_mutex);
  auto awaitable = m_queue.Enqueue(item);

  if (nullptr != m_exe_gate)
    m_exe_gate->AddPending();

  lock.unlock();

  co_await awaitable;
  co_return tx.Finish();
}

PullAPB::CommandGenerator PullAPB::GenerateCommands() {
  PulledCommand *commands_dst = nullptr;
  uint32_t *addr_dst = nullptr;
  uint32_t *data_dst = nullptr;
  size_t num_commands_left = 0;

  size_t accumulated = 0;
  auto refresh = [&]() { accumulated = 0; };

  std::tie(commands_dst, addr_dst, data_dst, num_commands_left) =
      co_yield accumulated;
  refresh();

  std::unique_lock lock(m_mutex, std::defer_lock_t{});

  while (true) {
    lock.lock();

    if (m_queue.NoScheduled()) {
      lock.unlock();

      std::tie(commands_dst, addr_dst, data_dst, num_commands_left) =
          co_yield accumulated;
      refresh();

      continue;
    }

    auto &item = m_queue.StartNextScheduled();
    lock.unlock();

    uint8_t first_in_xact = 1;

    for (auto action : item->tx.Incomplete()) {
      while (0 == num_commands_left) {
        std::tie(commands_dst, addr_dst, data_dst, num_commands_left) =
            co_yield accumulated;
        refresh();
      }

      action.Visit(Overload{
          [&](const SkipCycles &skip_c, SkipCycles::Out &) {
            *(commands_dst++) = PulledCommand{PSkipCycles, first_in_xact};
            *(addr_dst++) = 0;
            *(data_dst++) = skip_c.num_cycles;
          },
          [&](const APBWrite &apb_write, APBWrite::Out &) {
            *(commands_dst++) = PulledCommand{PWrite, first_in_xact};
            *(addr_dst++) = apb_write.address;
            *(data_dst++) = apb_write.data;
          },
          [&](const APBRead &apb_read, APBRead::Out &out) {
            *(commands_dst++) = PulledCommand{PRead, first_in_xact};
            *(addr_dst++) = apb_read.address;
            *(data_dst++) = 0;
            out.data = 0;
          },
          [&](const APBSetPSEL &set_psel, APBSetPSEL::Out &) {
            *(commands_dst++) = PulledCommand{PSetPSEL, first_in_xact};
            *(addr_dst++) = 0;
            *(data_dst++) = set_psel.psel;
          },
      });

      num_commands_left--;
      accumulated++;
      first_in_xact = 0;
    }
  }
}

PullAPB::ResultGenerator PullAPB::ConsumeResults() {
  const PushedStatus *status_src = 0;
  const uint32_t *data_src = 0;
  size_t num_results_left = 0;

  size_t consumed = 0;

  auto refresh = [&]() { consumed = 0; };

  std::tie(status_src, data_src, num_results_left) = co_yield consumed;
  refresh();

  std::unique_lock lock(m_mutex);

  while (true) {
    if (m_queue.NoInProgress()) {
      lock.unlock();

      std::tie(status_src, data_src, num_results_left) = co_yield consumed;
      refresh();

      lock.lock();
      continue;
    }

    auto &item = m_queue.GetCurrentInProgress();

    bool failed = false;
    for (auto action : item->tx.Incomplete()) {
      while (0 == num_results_left) {
        lock.unlock();

        std::tie(status_src, data_src, num_results_left) = co_yield consumed;
        refresh();

        lock.lock();
      }

      PushedStatus status = *(status_src++);
      uint32_t data = *(data_src++);
      num_results_left--;
      consumed++;

      if (failed)
        continue;

      if (PStatusError == status) {
        item->tx.Fail<CauseTargetError>();
        failed = true;
        continue;
      }

      if (PStatusTimeout == status) {
        item->tx.Fail<CauseTimeoutInCycles>(data);
        failed = true;
        continue;
      }

      auto *read_out = action.Out<APBRead>();
      if (nullptr != read_out)
        read_out->data = data;

      item->tx.Done(action);
    }

    if (nullptr != m_exe_gate)
      m_exe_gate->RemovePending();

    auto resolvable = m_queue.PrepareToResolve();
    lock.unlock();

    resolvable.Resolve();

    lock.lock();
  }
}

} // namespace edr
