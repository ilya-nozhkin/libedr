#ifndef LIBEDR_DRIVER_JTAG_PULLJTAG_H
#define LIBEDR_DRIVER_JTAG_PULLJTAG_H

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/execution_gate/ExecutionGate.h"
#include "libedr/driver/jtag/Jtag.hpp"
#include "libedr/util/adt/BitStream.hpp"
#include "libedr/util/asynchronicity/Asynchronicity.hpp"
#include "libedr/util/asynchronicity/ResolutionQueue.hpp"

namespace edr {

class PullJtag final : public Jtag {
  using BitStorage = uint8_t;

public:
  PullJtag(const DriverContext &ctx, std::string_view name,
           ExecutionGateImpl *exe_gate = nullptr);

  ~PullJtag() { Terminate(); }

  void Terminate() override;

  bool Serve(bool /*wait_if_empty*/) override { return false; }

  void Join(const std::coroutine_handle<> &to_complete) override;

  size_t PullTMSTDI(BitStream<BitStorage> &tms_dest,
                    BitStream<BitStorage> &tdi_dest);

  size_t PushTDO(BitStream<const BitStorage> &tdo_source);

  ExecutionGateImpl *GetExecutionGate() { return m_exe_gate; }

private:
  struct QueueItem {
    TxInProgress &tx;
  };

  CheckedTask<Status> Execute(TxInProgress &&tx) override;

  using TMSTDIGenerator = Generator<
      GeneratorArguments<BitStream<BitStorage> *, BitStream<BitStorage> *>,
      size_t>;

  using TDOGenerator =
      Generator<GeneratorArguments<BitStream<const BitStorage> *>, size_t>;

  using TransactionQueue = ResolutionQueue<QueueItem>;

  TMSTDIGenerator GenerateTMSTDI();
  TDOGenerator GenerateTDO();

  ExecutionGateImpl *m_exe_gate;

  std::mutex m_mutex;

  TransactionQueue m_queue;

  TMSTDIGenerator m_tms_tdi_generator;
  TDOGenerator m_tdo_generator;
};

} // namespace edr

#endif
