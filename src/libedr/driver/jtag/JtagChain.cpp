#include "libedr/driver/jtag/JtagChain.h"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/driver/jtag/JtagAction.hpp"
#include "libedr/driver/jtag/JtagChainAction.hpp"
#include "libedr/util/adt/BitStream.hpp"

#include <limits>
#include <mutex>

namespace edr {

namespace {

struct JCTransition {
  uint32_t bits = 0;
  uint32_t num_bits = std::numeric_limits<uint32_t>::max();
};

constexpr unsigned JCStateID(JCState state) {
  return static_cast<unsigned>(state);
}

using JCTransitions =
    std::array<std::array<JCTransition, JCStateID(JCState::Unknown)>,
               JCStateID(JCState::Unknown)>;

inline constexpr std::array<std::array<JCState, 2>, JCStateID(JCState::Unknown)>
    g_jc_steps = {{
        // TestLogicReset
        {JCState::RunTestIdle, JCState::TestLogicReset},
        // RunTestIdle
        {JCState::RunTestIdle, JCState::SelectDR},

        // SelectDR
        {JCState::CaptureDR, JCState::SelectIR},
        // CaptureDR
        {JCState::ShiftDR, JCState::Exit1DR},
        // ShiftDR
        {JCState::ShiftDR, JCState::Exit1DR},
        // Exit1DR
        {JCState::PauseDR, JCState::UpdateDR},
        // PauseDR
        {JCState::PauseDR, JCState::Exit2DR},
        // Exit2DR
        {JCState::ShiftDR, JCState::UpdateDR},
        // UpdateDR
        {JCState::RunTestIdle, JCState::SelectDR},

        // SelectIR
        {JCState::CaptureIR, JCState::TestLogicReset},
        // CaptureIR
        {JCState::ShiftIR, JCState::Exit1IR},
        // ShiftIR
        {JCState::ShiftIR, JCState::Exit1IR},
        // Exit1IR
        {JCState::PauseIR, JCState::UpdateIR},
        // PauseIR
        {JCState::PauseIR, JCState::Exit2IR},
        // Exit2IR
        {JCState::ShiftIR, JCState::UpdateIR},
        // UpdateIR
        {JCState::RunTestIdle, JCState::SelectDR},
    }};

inline constexpr uint64_t g_zeros = 0;
inline constexpr uint64_t g_ones = 0xFFFFFFFFFFFFFFFFllu;
inline constexpr uint64_t g_unstable = 0xCCCCCCCC;

inline constexpr std::array<uint64_t, JCStateID(JCState::Unknown)>
    g_waiting_sequences = {
        // TestLogicReset
        g_ones,
        // RunTestIdle
        g_zeros,

        // SelectDR
        g_unstable,
        // CaptureDR
        g_unstable,
        // ShiftDR
        g_zeros,
        // Exit1DR
        g_unstable,
        // PauseDR
        g_zeros,
        // Exit2DR
        g_unstable,
        // UpdateDR
        g_unstable,

        // SelectIR
        g_unstable,
        // CaptureIR
        g_unstable,
        // ShiftIR
        g_zeros,
        // Exit1IR
        g_unstable,
        // PauseIR
        g_zeros,
        // Exit2IR
        g_unstable,
        // UpdateIR
        g_unstable,
};

inline constexpr void ComputeTransitionsImpl(JCTransitions &transitions,
                                             JCState origin, JCState current,
                                             uint32_t bits, uint32_t num_bits) {
  auto &transition = transitions[JCStateID(origin)][JCStateID(current)];
  if (num_bits > transition.num_bits)
    return;

  transition.bits = bits;
  transition.num_bits = num_bits;

  auto [on0, on1] = g_jc_steps[JCStateID(current)];

  ComputeTransitionsImpl(transitions, origin, on0, bits, num_bits + 1);
  ComputeTransitionsImpl(transitions, origin, on1, bits | (1U << num_bits),
                         num_bits + 1);
};

inline constexpr JCTransitions ComputeTransitions() {
  JCTransitions transitions = {};
  for (unsigned i = 0; i < JCStateID(JCState::Unknown); i++) {
    auto state = static_cast<JCState>(i);
    ComputeTransitionsImpl(transitions, state, state, 0, 0);
  }

  return transitions;
};

inline constexpr JCTransitions g_jc_transitions = ComputeTransitions();

}; // namespace

class JtagChain::Visitor {
public:
  using SourceIterator = TxInProgress::Iterator;

  Visitor(JtagChain &chain, JtagChain::TxInProgress &ctx,
          Jtag::Builder &jbuilder, ChainState &state)
      : m_chain(chain), m_ctx(ctx), m_jbuilder(&jbuilder), m_state(state),
        m_tms_stream(NewTMSStream()) {}

  Visitor(JtagChain &chain, JtagChain::TxInProgress &ctx, Jtag::Status &jstatus,
          Jtag::Status::Iterator jbegin, ChainState &state)
      : m_chain(chain), m_ctx(ctx), m_jstatus(&jstatus), m_jit(jbegin),
        m_jend(jstatus.Complete().end()), m_state(state),
        m_tms_stream(NewTMSStream()) {}

  bool operator()(JCForgetChainStructure &in, JCForgetChainStructure::Out &out,
                  const SourceIterator &sit) {
    if (!FlushTDI(sit))
      return false;

    if (nullptr != m_jbuilder) {
      m_chain.m_num_taps = 1;
      m_chain.m_ir_lengths[0] = 1;
      m_chain.m_state.selected_tap = 0;
    }

    m_state.selected_tap = 0;

    if (nullptr == m_jbuilder)
      m_ctx.Done(sit);

    return true;
  }

  bool operator()(JCSetIRLength &in, JCSetIRLength::Out &out,
                  const SourceIterator &sit) {
    if (in.length > g_max_ir_length) {
      m_ctx.FailAt<CauseIRLengthTooBig>(sit, in.length, g_max_ir_length);
      return false;
    }

    if (!FlushTDI(sit))
      return false;

    if (in.tap_id >= g_max_num_taps) {
      m_ctx.FailAt<CauseInvalidJtagTapID>(sit, in.tap_id, g_max_num_taps - 1);
      return false;
    }

    if (nullptr != m_jbuilder) {
      for (unsigned i = m_chain.m_num_taps; i < in.tap_id; i++)
        m_chain.m_ir_lengths[i] = 1;

      m_chain.m_num_taps = in.tap_id + 1;
      m_chain.m_ir_lengths[in.tap_id] = in.length;
    }

    if (nullptr == m_jbuilder)
      m_ctx.Done(sit);

    return true;
  }

  bool operator()(JCGoToState &in, JCGoToState::Out &out,
                  const SourceIterator &sit) {
    if (!FlushTDI(sit))
      return false;

    assert(m_state.state < JCState::Unknown);

    if (in.state >= JCState::Unknown) {
      m_ctx.FailAt<CauseInvalidJtagState>(sit, JCStateID(in.state));
      return false;
    }

    if (!GoToState(in.state))
      return false;

    uint32_t to_wait = in.wait_there_cycles;
    uint64_t waiting_sequence = g_waiting_sequences[JCStateID(m_state.state)];

    if (to_wait != 0 && waiting_sequence == g_unstable) {
      m_ctx.FailAt<CauseUnstableJtagState>(sit, m_state.state);
      return false;
    }

    while (to_wait != 0) {
      if (0 == m_tms_stream.GetNumBits())
        if (!FlushTMS())
          return false;

      uint32_t this_time =
          std::min<uint32_t>(to_wait, m_tms_stream.GetNumBits());
      m_tms_stream.Write(waiting_sequence, this_time);

      to_wait -= this_time;
    }

    if (nullptr == m_jbuilder)
      m_ctx.Done(sit);

    return true;
  }

  bool operator()(JCSelectTAP &in, JCSelectTAP::Out &out,
                  const SourceIterator &sit) {
    if (in.tap_id >= m_chain.m_num_taps) {
      m_ctx.FailAt<CauseInvalidJtagTapID>(sit, in.tap_id, m_chain.m_num_taps);
      return false;
    }

    m_state.selected_tap = in.tap_id;

    if (nullptr == m_jbuilder)
      m_ctx.Done(sit);

    return true;
  }

  bool operator()(JCWriteIR &in, JCWriteIR::Out &out,
                  const SourceIterator &sit) {
    bool need_to_flush_tdi =
        m_current_tdi_operation != ActionID::InvalidAction &&
            m_current_tdi_operation != ActionID::JCWriteIR ||
        m_current_tdi_operation == ActionID::JCWriteIR &&
            m_state.selected_tap >= m_last_tdied_tap;

    if (need_to_flush_tdi && !FlushTDI(sit))
      return false;

    assert(m_state.selected_tap < m_chain.m_num_taps);

    if (m_current_tdi_operation == ActionID::InvalidAction) {
      m_selected_tap_on_first_tdi_sit = m_state.selected_tap;
      m_first_tdi_sit.emplace(sit);
      m_current_tdi_operation = ActionID::JCWriteIR;
      m_last_tdied_tap = m_chain.m_num_taps;
      m_total_tdi = 0;

      m_maybe_single_ir =
          SingleIR{.tap_id = m_state.selected_tap, .ir_value = in.value};
    } else
      m_maybe_single_ir.reset();

    while (m_last_tdied_tap != m_state.selected_tap) {
      m_last_tdied_tap--;
      m_total_tdi += m_chain.m_ir_lengths[m_last_tdied_tap];
    }

    return true;
  }

  template <class T>
  bool operator()(T &in, T::Out &out, const SourceIterator &sit) {
    static_assert(std::is_same_v<T, JCWriteDR> || std::is_same_v<T, JCShiftDR>);

    bool need_to_flush_tdi =
        m_current_tdi_operation != ActionID::InvalidAction &&
            m_current_tdi_operation != T::g_id ||
        m_current_tdi_operation == T::g_id &&
            m_state.selected_tap > m_last_tdied_tap;

    if (need_to_flush_tdi && !FlushTDI(sit))
      return false;

    assert(m_state.selected_tap < m_chain.m_num_taps);

    if (m_current_tdi_operation == ActionID::InvalidAction) {
      m_selected_tap_on_first_tdi_sit = m_state.selected_tap;
      m_first_tdi_sit.emplace(sit);
      m_current_tdi_operation = T::g_id;
      m_last_tdied_tap = m_chain.m_num_taps;
      m_total_tdi = 0;
    }

    // Bypass all TAPs in between.
    if (m_last_tdied_tap != m_state.selected_tap) {
      m_total_tdi += (m_last_tdied_tap - 1) - m_state.selected_tap;
      m_last_tdied_tap = m_state.selected_tap;
    }

    m_total_tdi += in.Bits().GetNumBits();
    return true;
  }

  bool FlushTMS() {
    size_t to_flush = g_max_tms_bits - m_tms_stream.GetNumBits();
    if (0 == to_flush)
      return true;

    if (nullptr != m_jbuilder) {
      BitStream<uint64_t> read_stream(&m_tms_buffer, to_flush);
      m_jbuilder->Add<PutTMS>(read_stream);
    } else {
      if (*m_jit == *m_jend) {
        m_ctx.Fail<CauseNestedError>(m_jstatus->GetError());
        return false;
      }

      (*m_jit)++;
    }

    m_tms_stream = NewTMSStream();
    return true;
  }

  bool FlushTDI(const SourceIterator &send) {
    if (m_current_tdi_operation == ActionID::InvalidAction)
      return true;

    bool writing_ir = m_current_tdi_operation == ActionID::JCWriteIR;
    if (writing_ir && m_maybe_single_ir == m_state.last_written_single_ir) {
      if (nullptr == m_jbuilder)
        for (auto it = *m_first_tdi_sit; it != send; it++)
          m_ctx.Done(it);

      m_first_tdi_sit.reset();
      m_current_tdi_operation = ActionID::InvalidAction;
      return true;
    }

    auto target_state = writing_ir ? JCState::ShiftIR : JCState::ShiftDR;
    if (!GoToState(target_state))
      return false;

    if (!FlushTMS())
      return false;

    if (nullptr != m_jbuilder) {
      auto dest = [&]() -> std::optional<BitStream<uint32_t>> {
        if (m_current_tdi_operation == ActionID::JCShiftDR) {
          auto tpos = m_jbuilder->Add<PutTDIGetTDO>(m_total_tdi, 1u);
          auto *action = m_jbuilder->Get(tpos);
          if (nullptr == action)
            return {};

          return action->Bits();
        }

        auto tpos = m_jbuilder->Add<PutTDI>(m_total_tdi, 1u);
        auto *action = m_jbuilder->Get(tpos);
        if (nullptr == action)
          return {};

        return action->Bits();
      }();

      auto it = *m_first_tdi_sit;
      unsigned selected_tap = m_selected_tap_on_first_tdi_sit;
      unsigned last_shifted_tap = m_chain.m_num_taps;

      for (; it != send; it++) {
        auto [select_tap, _] = it->As<JCSelectTAP>();
        if (nullptr != select_tap) {
          selected_tap = select_tap->tap_id;
          continue;
        }

        if (m_current_tdi_operation == ActionID::JCWriteIR) {
          auto [write_ir, __] = it->As<JCWriteIR>();

          last_shifted_tap--;

          while (last_shifted_tap != selected_tap) {
            static_assert(g_max_ir_length <= 8 * sizeof(g_ones));
            dest->Write(g_ones, m_chain.m_ir_lengths[last_shifted_tap]);
            last_shifted_tap--;
          }

          dest->Write(write_ir->value, m_chain.m_ir_lengths[last_shifted_tap]);

          continue;
        }

        auto [write_dr, __] = it->As<JCWriteDR>();
        auto [shift_dr, ___] = it->As<JCShiftDR>();

        auto source = write_dr != nullptr ? write_dr->Bits() : shift_dr->Bits();

        unsigned zeros_to_add = last_shifted_tap != selected_tap
                                    ? last_shifted_tap - 1 - selected_tap
                                    : 0;
        while (zeros_to_add > 0) {
          unsigned this_time =
              std::min<unsigned>(zeros_to_add, 8 * sizeof(g_zeros));
          dest->Write(g_zeros, this_time);
          zeros_to_add -= this_time;
        }

        dest->Write(source, source.GetNumBits());
        last_shifted_tap = selected_tap;
      }

      if (m_current_tdi_operation == ActionID::JCWriteIR) {
        while (last_shifted_tap-- > 0)
          dest->Write(g_ones, m_chain.m_ir_lengths[last_shifted_tap]);
      } else {
        unsigned zeros_to_add = last_shifted_tap;
        while (zeros_to_add > 0) {
          unsigned this_time =
              std::min<unsigned>(zeros_to_add, 8 * sizeof(g_zeros));
          dest->Write(g_zeros, this_time);
          zeros_to_add -= this_time;
        }
      }
    } else {
      if (*m_jit == m_jend) {
        m_ctx.Undo(*m_first_tdi_sit);
        m_ctx.Fail<CauseNestedError>(m_jstatus->GetError());
        return false;
      }

      std::optional<BitStream<uint32_t>> tdo;

      auto *tdo_out = (*m_jit)->Out<PutTDIGetTDO>();
      if (nullptr != tdo_out)
        tdo.emplace(tdo_out->Bits());

      (*m_jit)++;

      unsigned selected_tap = m_selected_tap_on_first_tdi_sit;
      unsigned last_shifted_tap = m_chain.m_num_taps;

      auto it = *m_first_tdi_sit;
      for (; it != send; it++) {
        auto [select_tap, _] = it->As<JCSelectTAP>();
        if (nullptr != select_tap) {
          selected_tap = select_tap->tap_id;
          m_ctx.Done(it);
          continue;
        }

        auto [shift_dr, shift_dr_out] = it->As<JCShiftDR>();
        if (nullptr != shift_dr_out) {
          unsigned to_skip = last_shifted_tap != selected_tap
                                 ? last_shifted_tap - 1 - selected_tap
                                 : 0;
          tdo->Skip(to_skip);

          shift_dr_out->num_bits = shift_dr->num_bits;
          shift_dr_out->Bits().Write(*tdo, shift_dr_out->num_bits);
        }

        m_ctx.Done(it);
        last_shifted_tap = selected_tap;
      }
    }

    m_state.state =
        m_state.state == JCState::ShiftIR ? JCState::Exit1IR : JCState::Exit1DR;

    m_state.last_written_single_ir = m_maybe_single_ir;
    m_first_tdi_sit.reset();
    m_current_tdi_operation = ActionID::InvalidAction;
    return true;
  }

private:
  BitStream<uint64_t> NewTMSStream() {
    return BitStream<uint64_t>(&m_tms_buffer, g_max_tms_bits);
  }

  bool GoToState(JCState state) {
    auto transition =
        g_jc_transitions[JCStateID(m_state.state)][JCStateID(state)];

    if (m_tms_stream.GetNumBits() < transition.num_bits)
      if (!FlushTMS())
        return false;

    assert(m_tms_stream.GetNumBits() >= transition.num_bits);
    m_tms_stream.Write(transition.bits, transition.num_bits);

    m_state.state = state;
    return true;
  }

  bool m_translating;
  JtagChain &m_chain;
  JtagChain::TxInProgress &m_ctx;

  Jtag::Builder *m_jbuilder = nullptr;
  Jtag::Status *m_jstatus = nullptr;
  std::optional<Jtag::Status::Iterator> m_jit;
  std::optional<Jtag::Status::Iterator> m_jend;

  ChainState &m_state;

  uint64_t m_tms_buffer;
  BitStream<uint64_t> m_tms_stream;

  static constexpr size_t g_max_tms_bits = 8 * sizeof(m_tms_buffer);

  unsigned m_selected_tap_on_first_tdi_sit = 0;
  std::optional<SourceIterator> m_first_tdi_sit;
  ActionID m_current_tdi_operation = ActionID::InvalidAction;
  unsigned m_last_tdied_tap = 0;
  uint32_t m_total_tdi = 0;
  std::optional<SingleIR> m_maybe_single_ir;
};

JtagChain::CheckedTask<JtagChain::Status>
JtagChain::Execute(TxInProgress &&tx) {
  auto jbuilder = m_jtag.Initiate(tx, "Shift bits");

  std::unique_lock<std::mutex> lock(m_mutex);

  ChainState old_state = m_state;
  ChainState new_state = m_state;

  if (new_state.state == JCState::Unknown) {
    BitStream<const uint32_t> reset_seq(0b11111u, 5u);
    jbuilder.Add<PutTMS>(reset_seq);
    new_state.state = JCState::TestLogicReset;
  }

  Visitor translator(*this, tx, jbuilder, new_state);

  auto incomplete = tx.Incomplete();

  for (auto it = incomplete.begin(); it != incomplete.end(); it++) {
    std::optional<bool> success = it->Visit(translator, it);
    if (!success || !*success)
      co_return tx.Finish();
  }

  if (!translator.FlushTDI(incomplete.end()) || !translator.FlushTMS())
    co_return tx.Finish();

  m_state = new_state;
  auto task = m_jtag.Schedule(std::move(jbuilder));

  lock.unlock();

  auto jstatus = co_await task;

  lock.lock();

  auto jcomplete = jstatus.Complete();
  auto jbegin = jcomplete.begin();

  if (old_state.state == JCState::Unknown) {
    if (jbegin == jcomplete.end()) {
      tx.Fail<CauseNestedError>(jstatus.GetError());
      m_state.state = JCState::Unknown;
      co_return tx.Finish();
    }

    jbegin++;
    old_state.state = JCState::TestLogicReset;
  }

  Visitor completer(*this, tx, jstatus, jbegin, old_state);
  for (auto it = incomplete.begin(); it != incomplete.end(); it++) {
    std::optional<bool> success = it->Visit(completer, it);
    if (!success || !*success) {
      m_state.state = JCState::Unknown;
      co_return tx.Finish();
    }
  }

  if (!completer.FlushTDI(incomplete.end()) || !completer.FlushTMS())
    m_state.state = JCState::Unknown;

  co_return tx.Finish();
}

} // namespace edr
