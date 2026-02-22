#include "libedr/driver/jtag/PassiveJtag.h"
#include "libedr/driver/Error.hpp"
#include "libedr/driver/jtag/JtagAction.hpp"
#include "libedr/util/adt/BitStream.hpp"
#include <mutex>

namespace edr {

PassiveJtag::PassiveJtag(const DriverContext &ctx, std::string_view name,
                         ExecutionGate *exe_gate)
    : Jtag(ctx, name), m_exe_gate(exe_gate), m_queue(*this),
      m_in_progress(GetFrameAllocator()), m_tms_tdi_generator(GenerateTMSTDI()),
      m_tdo_generator(GenerateTDO()) {}

void PassiveJtag::Terminate() {
  std::unique_lock<std::mutex> lock(m_mutex);
  while (!m_in_progress.empty() || !m_queue.Empty()) {
    auto fail = [&](auto &item) {
      item->tx.template Fail<CauseTerminated>(m_name);

      lock.unlock();
      item.Resolve();
      lock.lock();
    };

    if (!m_in_progress.empty()) {
      auto item = std::move(m_in_progress.front());
      m_in_progress.pop();

      fail(item);
    }

    if (!m_queue.Empty()) {
      auto item = m_queue.Pop();
      fail(item);
    }
  }
}

void PassiveJtag::Join(const std::coroutine_handle<> &to_complete) {}

size_t PassiveJtag::PullTMSTDI(BitStream<BitStorage> &tms_dest,
                               BitStream<BitStorage> &tdi_dest) {
  return m_tms_tdi_generator(&tms_dest, &tdi_dest);
}

size_t PassiveJtag::PushTDO(BitStream<const BitStorage> &tdo_source) {
  return m_tdo_generator(&tdo_source);
}

PassiveJtag::CheckedTask<PassiveJtag::Status>
PassiveJtag::Execute(TxInProgress &&tx) {
  std::unique_lock<std::mutex> lock(m_mutex);

  auto awaitable = m_queue.Emplace(tx);

  lock.unlock();

  if (nullptr != m_exe_gate)
    m_exe_gate->AddPending();

  co_await awaitable;
  co_return tx.Finish();
}

PassiveJtag::TMSTDIGenerator PassiveJtag::GenerateTMSTDI() {
  static constexpr uint64_t g_zeros = 0;
  static constexpr uint64_t g_ones = 0xFFFFFFFFFFFFFFFFllu;

  BitStream<BitStorage> *tms_dest = nullptr;
  BitStream<BitStorage> *tdi_dest = nullptr;

  size_t accumulated = 0;

  auto refresh = [&]() { accumulated = 0; };

  auto try_write = [&accumulated](BitStream<BitStorage> &data_dest,
                                  BitStream<BitStorage> &filler_dest,
                                  BitStream<const uint32_t> &data_source,
                                  auto filler_source) -> bool {
    if (filler_dest.GetNumBits() < data_dest.GetNumBits())
      data_dest.Crop(filler_dest.GetNumBits());

    size_t to_write =
        std::min(data_dest.GetNumBits(), data_source.GetNumBits());
    data_dest.Write(data_source, to_write);

    size_t to_fill = to_write;
    while (to_fill != 0) {
      size_t this_time = std::min(8 * sizeof(filler_source), to_fill);
      filler_dest.Write(filler_source, this_time);

      to_fill -= this_time;
    }

    accumulated += to_write;
    return 0 == data_source.GetNumBits();
  };

  std::tie(tms_dest, tdi_dest) = co_yield accumulated;
  refresh();

  std::unique_lock lock(m_mutex, std::defer_lock_t{});

  while (true) {
    lock.lock();

    if (m_queue.Empty()) {
      lock.unlock();

      std::tie(tms_dest, tdi_dest) = co_yield accumulated;
      refresh();

      continue;
    }

    auto &item = m_in_progress.emplace(m_queue.Pop());
    lock.unlock();

    for (auto action : item->tx.Incomplete()) {
      auto [put_tms, _] = action.As<PutTMS>();
      auto [put_tdi, __] = action.As<PutTDI>();
      auto [put_tdi_get_tdo, ___] = action.As<PutTDIGetTDO>();

      if (nullptr != put_tms) {
        auto tms_source = put_tms->Bits();

        while (!try_write(*tms_dest, *tdi_dest, tms_source, g_zeros)) {
          std::tie(tms_dest, tdi_dest) = co_yield accumulated;
          refresh();
        }

        continue;
      }

      auto tdi_source =
          nullptr != put_tdi ? put_tdi->Bits() : put_tdi_get_tdo->Bits();
      uint32_t last_tms =
          nullptr != put_tdi ? put_tdi->last_tms : put_tdi_get_tdo->last_tms;

      if (0 == tdi_source.GetNumBits())
        continue;

      if (0 == last_tms) {
        while (!try_write(*tdi_dest, *tms_dest, tdi_source, g_zeros)) {
          std::tie(tms_dest, tdi_dest) = co_yield accumulated;
          refresh();
        }
      } else {
        auto [left, right] = tdi_source.Split(tdi_source.GetNumBits() - 1);
        while (!try_write(*tdi_dest, *tms_dest, left, g_zeros)) {
          std::tie(tms_dest, tdi_dest) = co_yield accumulated;
          refresh();
        }

        while (!try_write(*tdi_dest, *tms_dest, right, g_ones)) {
          std::tie(tms_dest, tdi_dest) = co_yield accumulated;
          refresh();
        }
      }
    }
  }
}

PassiveJtag::TDOGenerator PassiveJtag::GenerateTDO() {
  BitStream<const BitStorage> *tdo_source = nullptr;

  size_t consumed = 0;

  auto refresh = [&]() { consumed = 0; };

  tdo_source = co_yield consumed;
  refresh();

  std::unique_lock lock(m_mutex);

  while (true) {
    if (m_in_progress.empty()) {
      lock.unlock();

      tdo_source = co_yield consumed;
      refresh();

      lock.lock();
      continue;
    }

    auto &item = m_in_progress.front();
    lock.unlock();

    for (auto action : item->tx.Incomplete()) {
      auto [put_tms, put_tms_out] = action.As<PutTMS>();
      auto [put_tdi, put_tdi_out] = action.As<PutTDI>();
      auto [put_tdi_get_tdo, put_tdi_get_tdo_out] = action.As<PutTDIGetTDO>();

      if (nullptr != put_tms_out || nullptr != put_tdi) {
        auto &num_put =
            nullptr != put_tms ? put_tms_out->num_put : put_tdi_out->num_put;
        num_put = 0;

        size_t num_bits =
            nullptr != put_tms ? put_tms->num_bits : put_tdi->num_bits;

        while (num_bits != 0) {
          size_t this_time = tdo_source->Skip(num_bits);

          num_bits -= this_time;
          num_put += this_time;
          consumed += this_time;

          if (num_bits != 0) {
            tdo_source = co_yield consumed;
            refresh();
          }
        }

        item->tx.Done(action);
        continue;
      }

      assert(nullptr != put_tdi_get_tdo_out);
      auto tdo_dest = put_tdi_get_tdo_out->Bits(put_tdi_get_tdo->num_bits);
      put_tdi_get_tdo_out->num_put = 0;

      while (0 != tdo_dest.GetNumBits()) {
        size_t needed_before = tdo_dest.GetNumBits();
        tdo_dest.Write(*tdo_source, tdo_source->GetNumBits());

        size_t this_time = needed_before - tdo_dest.GetNumBits();

        put_tdi_get_tdo_out->num_put += this_time;
        consumed += this_time;

        if (0 != tdo_dest.GetNumBits()) {
          tdo_source = co_yield consumed;
          refresh();
        }
      }

      item->tx.Done(action);
    }

    if (nullptr != m_exe_gate)
      m_exe_gate->RemovePending();

    item.Resolve();

    lock.lock();
    m_in_progress.pop();
  }
}

} // namespace edr
