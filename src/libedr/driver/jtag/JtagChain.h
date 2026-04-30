#ifndef LIBEDR_DRIVER_JTAG_JTAGCHAIN_HPP
#define LIBEDR_DRIVER_JTAG_JTAGCHAIN_HPP

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/driver/jtag/Jtag.hpp"
#include "libedr/driver/jtag/JtagChainAction.hpp"

namespace edr {

class JtagChain : public Driver<DriverID::JtagChain, JtagChainAction> {
public:
  JtagChain(const DriverContext &ctx, std::string_view name, Jtag &jtag)
      : Driver(ctx, name), m_jtag(jtag) {}

  ~JtagChain() override = default;

  void Terminate() override { m_jtag.Terminate(); }

  bool Serve(bool wait_if_empty) override {
    return m_jtag.Serve(wait_if_empty);
  }

  void Join(const std::coroutine_handle<> &to_complete) override {
    m_jtag.Join(to_complete);
  }

private:
  static constexpr unsigned g_max_num_taps = 256;
  static constexpr unsigned g_max_ir_length = 8 * sizeof(JCWriteIR::value);

  struct SingleIR {
    unsigned tap_id;
    uint32_t ir_value;

    friend bool operator==(const SingleIR &left, const SingleIR &right) {
      return left.tap_id == right.tap_id && left.ir_value == right.ir_value;
    }
  };

  struct ChainState {
    JCState state = JCState::Unknown;
    unsigned selected_tap = 0;
    std::optional<SingleIR> last_written_single_ir;
  };

  class Visitor;

  CheckedTask<Status> Execute(TxInProgress &&tx) override;

  std::mutex m_mutex;
  Jtag &m_jtag;
  ChainState m_state;

  uint32_t m_ir_lengths[g_max_num_taps] = {1};
  size_t m_num_taps = 1;
};

} // namespace edr

#endif
