#ifndef LIBEDR_DRIVER_JTAG_PASSIVEJTAG_H
#define LIBEDR_DRIVER_JTAG_PASSIVEJTAG_H

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/jtag/Jtag.hpp"
#include "libedr/util/adt/BitStream.hpp"
#include "libedr/util/asynchronicity/Asynchronicity.hpp"
#include "libedr/util/asynchronicity/AsynchronousPrimitives.hpp"

namespace edr {

class PassiveJtag final : public Jtag {
  using BitStorage = uint8_t;

public:
  PassiveJtag(const DriverContext &ctx, std::string_view name);

  ~PassiveJtag() { Terminate(); }

  void Terminate() override;

  void Join(const std::coroutine_handle<> &to_complete) override;

  size_t PullTMSTDI(BitStream<BitStorage> &tms_dest,
                    BitStream<BitStorage> &tdi_dest);

  size_t PushTDO(BitStream<const BitStorage> &tdo_source);

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

  using TransactionQueue = ResolutionQueue<PassiveJtag, QueueItem>;

  TMSTDIGenerator GenerateTMSTDI();
  TDOGenerator GenerateTDO();

  std::mutex m_mutex;

  TransactionQueue m_queue;
  TransactionQueue::ProgressQueue m_in_progress;

  TMSTDIGenerator m_tms_tdi_generator;
  TDOGenerator m_tdo_generator;
};

} // namespace edr

#endif
