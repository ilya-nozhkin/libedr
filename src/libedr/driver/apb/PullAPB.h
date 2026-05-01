#ifndef LIBEDR_DRIVER_APB_PULLAPB_H
#define LIBEDR_DRIVER_APB_PULLAPB_H

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/apb/APB.hpp"
#include "libedr/driver/execution_gate/ExecutionGate.h"
#include "libedr/util/asynchronicity/Asynchronicity.hpp"
#include "libedr/util/asynchronicity/ResolutionQueue.hpp"

namespace edr {

class PullAPB final : public APB {
  using BitStorage = uint8_t;

public:
  enum PulledCommandType : uint8_t {
    PSkipCycles = 1,
    PWrite = 2,
    PRead = 3,
    PSetPSEL = 4,
  };

  struct PulledCommand {
    PulledCommandType command_type : 7;
    uint8_t first_in_xact : 1;
  };

  enum PushedStatus : uint8_t {
    PStatusOk = 0,
    PStatusError = 1,
    PStatusTimeout = 2,
  };

  PullAPB(const DriverContext &ctx, std::string_view name,
          ExecutionGateImpl *exe_gate = nullptr);

  ~PullAPB() { Terminate(); }

  void Terminate() override;

  bool Serve(bool /*wait_if_empty*/) override { return false; }

  void Join(const std::coroutine_handle<> &to_complete) override;

  size_t PullCommands(PulledCommand *commands_dst, uint32_t *addr_dst,
                      uint32_t *data_dst, size_t max_num_commands);

  size_t PushResults(const PushedStatus *status_src, const uint32_t *data_src,
                     size_t num_results);

  ExecutionGateImpl *GetExecutionGate() { return m_exe_gate; }

private:
  struct QueueItem {
    TxInProgress &tx;
  };

  CheckedTask<Status> Execute(TxInProgress &&tx) override;

  using CommandGenerator = Generator<
      GeneratorArguments<PulledCommand *, uint32_t *, uint32_t *, size_t>,
      size_t>;

  using ResultGenerator = Generator<
      GeneratorArguments<const PushedStatus *, const uint32_t *, size_t>,
      size_t>;

  using TransactionQueue = ResolutionQueue<QueueItem>;

  CommandGenerator GenerateCommands();
  ResultGenerator ConsumeResults();

  ExecutionGateImpl *m_exe_gate;

  std::mutex m_mutex;

  TransactionQueue m_queue;

  CommandGenerator m_command_generator;
  ResultGenerator m_result_consumer;
};

} // namespace edr

#endif
