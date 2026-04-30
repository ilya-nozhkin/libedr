#include "libedr/driver/riscv/RVJtagDTM.h"
#include "libedr/driver/CommonActions.hpp"
#include "libedr/driver/Error.hpp"
#include "libedr/driver/jtag/JtagChainAction.hpp"
#include "libedr/driver/riscv/RVDTMAction.hpp"

#include <cstdint>
#include <mutex>

namespace {

union DTMCSReg {
  struct {
    uint32_t version : 4;
    uint32_t abits : 6;
    uint32_t dmistat : 2;
    uint32_t idle : 3;
    uint32_t _ : 1;
    uint32_t dmireset : 1;
    uint32_t dtmhardreset : 1;
    uint32_t errinfo : 3;
    uint32_t __ : 11;
  } fields;
  uint32_t u32;
};

inline constexpr size_t g_dmi_op_bitsize = 2;
inline constexpr size_t g_dmi_data_bitsize = 32;
inline constexpr size_t g_dmi_addr_bitsize = 30;

union DMIReg {
  struct {
    uint64_t op : g_dmi_op_bitsize;
    uint64_t data : g_dmi_data_bitsize;
    uint64_t address : g_dmi_addr_bitsize;
  } fields;
  uint64_t u64;
};

static_assert(8 * sizeof(DMIReg) ==
              g_dmi_op_bitsize + g_dmi_data_bitsize + g_dmi_addr_bitsize);

enum DMIOp {
  DMIOpRead = 1,
  DMIOpWrite = 2,
};

enum DMIStatus {
  DMIStatusOk = 0,
  DMIStatusFail = 2,
  DMIStatusBusy = 3,
};

} // namespace

namespace edr {

RVJtagDTM::CheckedTask<RVJtagDTM::Status>
RVJtagDTM::Execute(TxInProgress &&tx) {
  std::unique_lock<std::mutex> lock(m_mutex);

  AsyncGate::Item gate_item;
  co_await m_discovery_gate.Pass(lock, gate_item);

  if (m_state.dtm_state == DTMState::Invalid) {
    tx.Fail<CauseTerminated>(GetName());
    co_return tx.Finish();
  }

  bool i_closed_the_gate = false;

  if (m_state.dtm_state == DTMState::Initial) {
    m_discovery_gate.Close(lock);
    i_closed_the_gate = true;

    auto discovered = co_await Discover(lock, tx);
    if (!discovered) {
      m_state.dtm_state = DTMState::Invalid;
      m_discovery_gate.Open(lock);
      co_return tx.Finish();
    }

    m_state.dtm_state = DTMState::Operating;
  }

  AttemptStatus prev_attempt = AttemptStatus::Success;
  while (true) {
    AttemptStatus status = co_await Attempt(lock, tx, prev_attempt);
    prev_attempt = status;

    if (status == AttemptStatus::Success) {
      m_config.latency.Enough();
      break;
    }

    if (status == AttemptStatus::MyFailure)
      break;

    if (status == AttemptStatus::MyBusy) {
      if (m_config.latency.IsMax()) {
        tx.Fail<CauseMaxLatencyReached>(m_config.latency.Get());
        m_state.dtm_state = DTMState::Invalid;
        break;
      }

      m_config.latency.TooLow();
    }
  }

  if (i_closed_the_gate)
    m_discovery_gate.Open(lock);

  co_return tx.Finish();
}

RVJtagDTM::Task<bool> RVJtagDTM::Discover(std::unique_lock<std::mutex> &lock,
                                          TxInProgress &tx) {
  DTMCSReg reset_command{};
  reset_command.fields.dtmhardreset = 1;
  reset_command.fields.dmireset = 1;

  auto xact = m_jtag.Initiate(tx, "Read dtmcs");
  xact.Add<JCSelectTAP>(m_config.tap_id);
  xact.Add<JCWriteIR>(m_config.dtmcs_ir_value);
  xact.Add<JCShiftDR>(reset_command.u32, 32);
  xact.Add<JCGoToState>(JCState::RunTestIdle);
  auto read_dtmcs = xact.Add<JCShiftDR>(0, 32);

  auto task = m_jtag.Schedule(std::move(xact));

  lock.unlock();
  auto result = co_await task;
  lock.lock();

  if (!result) {
    tx.Fail<CauseNestedError>(result.GetError());
    co_return false;
  }

  auto *read_dtmcs_out = result.Out(read_dtmcs);
  if (nullptr == read_dtmcs_out) {
    tx.Fail<CauseAllocationFailure>();
    co_return false;
  }

  DTMCSReg dtmcs{};
  dtmcs.u32 = read_dtmcs_out->Bits().Read<uint32_t>(32);

  if (dtmcs.fields.version == 0) {
    tx.Fail<CauseInvalidVersion>("RISC-V DTM",
                                 static_cast<uint32_t>(dtmcs.fields.version));
    co_return false;
  }

  if (dtmcs.fields.errinfo != 0) {
    tx.Fail<CauseFailedToClearErrorFlag>(
        "dtmcs.errinfo", static_cast<uint32_t>(dtmcs.fields.errinfo));
    co_return false;
  }

  if (dtmcs.fields.abits > g_dmi_addr_bitsize) {
    tx.Fail<CauseValueOutOfRange>("dtmcs.abits",
                                  static_cast<uint64_t>(dtmcs.fields.abits), 0,
                                  g_dmi_addr_bitsize);
    co_return false;
  }

  m_dmi_size = g_dmi_op_bitsize + g_dmi_data_bitsize + dtmcs.fields.abits;

  auto min_latency = dtmcs.fields.idle == 0 ? 0 : dtmcs.fields.idle - 1;
  m_config.latency.BumpMin(min_latency);

  co_return true;
}

RVJtagDTM::Task<RVJtagDTM::AttemptStatus>
RVJtagDTM::Attempt(std::unique_lock<std::mutex> &lock, TxInProgress &tx,
                   AttemptStatus prev_attempt) {
  auto xact = m_jtag.Initiate(tx, "Access DMI");
  xact.Add<JCSelectTAP>(m_config.tap_id);

  State state_before = m_state;
  State state = m_state;

  if (state.dtm_state == DTMState::Error) {
    xact.Add<JCWriteIR>(m_config.dtmcs_ir_value);

    DTMCSReg reset_error{};
    reset_error.fields.dmireset = 1;
    xact.Add<JCWriteDR>(reset_error.u32, 32);

    state.dtm_state = DTMState::Operating;
  }

  xact.Add<JCWriteIR>(m_config.dmi_ir_value);

  bool first_incomplete = true;
  bool waiting_for_first_incomplete = prev_attempt == AttemptStatus::MyBusy;

  uint32_t abits = m_dmi_size - g_dmi_op_bitsize - g_dmi_data_bitsize;
  uint64_t max_address = (1ull << abits) - 1;

  for (auto act : tx.Incomplete()) {
    auto [skip_c, _] = act.As<SkipCycles>();
    if (nullptr != skip_c) {
      xact.Add<JCGoToState>(JCState::RunTestIdle, skip_c->num_cycles);
      continue;
    }

    if (first_incomplete && waiting_for_first_incomplete) {
      first_incomplete = false;
      continue;
    }

    auto [write_dmi, __] = act.As<RVWriteDMI>();
    auto [read_dmi, ___] = act.As<RVReadDMI>();

    uint32_t address = 0;

    DMIReg dmi{};
    if (nullptr != write_dmi) {
      address = write_dmi->address;
      dmi.fields.data = write_dmi->data;
      dmi.fields.op = DMIOpWrite;
    } else {
      address = read_dmi->address;
      dmi.fields.op = DMIOpRead;
    }

    if (address > max_address) {
      tx.FailAt<CauseValueOutOfRange>(act, "DMI address", address, 0,
                                      max_address);
      co_return AttemptStatus::MyFailure;
    }

    dmi.fields.address = address;

    xact.Add<JCShiftDR>(dmi.u64, m_dmi_size);
    xact.Add<JCGoToState>(JCState::RunTestIdle, m_config.latency.Get());
  }

  xact.Add<JCShiftDR>(0ull, m_dmi_size);
  xact.Add<JCGoToState>(JCState::UpdateDR);

  m_state = state;
  auto task = m_jtag.Schedule(std::move(xact));

  lock.unlock();
  auto jstatus = co_await task;
  lock.lock();

  auto jcomplete = jstatus.Complete();
  auto jit = jcomplete.begin();
  auto jend = jcomplete.end();

  auto check_fail_inc = [&]() -> bool {
    if (jit == jend) {
      m_state.dtm_state = DTMState::Invalid;
      tx.Fail<CauseNestedError>(jstatus.GetError());
      return true;
    }

    ++jit;
    return false;
  };

  check_fail_inc(); // JCSelectTAP

  if (state_before.dtm_state == DTMState::Error)
    if (check_fail_inc() || check_fail_inc())
      co_return AttemptStatus::MyFailure;

  check_fail_inc(); // JCWriteIR(dmi)

  first_incomplete = true;

  for (auto act : tx.Incomplete()) {
    auto [skip_c, _] = act.As<SkipCycles>();
    if (nullptr != skip_c) {
      if (check_fail_inc())
        co_return AttemptStatus::MyFailure;

      tx.Done(act);
      continue;
    }

    if (first_incomplete && !waiting_for_first_incomplete) {
      if (check_fail_inc() || check_fail_inc())
        co_return AttemptStatus::MyFailure;

      first_incomplete = false;
    }

    if (jit == jend) {
      m_state.dtm_state = DTMState::Invalid;
      tx.Fail<CauseNestedError>(jstatus.GetError());
      co_return AttemptStatus::MyFailure;
    }

    DMIReg dmi_out{};
    auto *out = jit->Out<JCShiftDR>();
    dmi_out.u64 = out->Bits().Read<uint64_t>(m_dmi_size);

    ++jit;
    check_fail_inc();

    if (dmi_out.fields.op == DMIStatusOk) {
      auto *read_out = act.Out<RVReadDMI>();
      if (nullptr != read_out)
        read_out->data = dmi_out.fields.data;

      tx.Done(act);
      continue;
    }

    bool my_error = state_before.num_errors == m_state.num_errors;
    if (my_error) {
      m_state.num_errors++;
      m_state.dtm_state = DTMState::Error;
    }

    if (dmi_out.fields.op == DMIStatusBusy)
      co_return my_error ? AttemptStatus::MyBusy : AttemptStatus::PrevBusy;

    tx.Fail<CauseTargetError>();
    co_return my_error ? AttemptStatus::MyFailure : AttemptStatus::PrevFailure;
  }

  co_return AttemptStatus::Success;
}

} // namespace edr
