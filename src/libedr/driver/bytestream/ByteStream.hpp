#ifndef LIBEDR_DRIVER_BYTESTREAM_BYTESTREAM_HPP
#define LIBEDR_DRIVER_BYTESTREAM_BYTESTREAM_HPP

#include "libedr/driver/PushQueueEndpoint.hpp"
#include "libedr/driver/Driver.hpp"
#include "libedr/driver/Error.hpp"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/driver/bytestream/ByteStreamAction.hpp"

#include <algorithm>
#include <cstddef>

namespace edr {

using ByteStream = Driver<DriverID::ByteStream, ByteStreamAction>;

class BlockingByteStream {
public:
  virtual ~BlockingByteStream() = default;

  virtual std::pair<size_t, Error> Write(std::span<const std::byte> source) = 0;

  virtual std::pair<size_t, Error> Read(std::span<std::byte> dest) = 0;

  virtual void Terminate() = 0;
};

class DeferredByteStream final : public ByteStream {
public:
  DeferredByteStream(const DriverContext &ctx, std::string_view name,
                     BlockingByteStream &stream)
      : ByteStream(ctx, name), m_stream(stream),
        m_write_buffer(ctx.TransactionBufferResource()) {}

  ~DeferredByteStream() override { Terminate(); }

  bool Serve(bool wait_if_empty) {
    return m_endpoint.Serve(wait_if_empty, [this](PendingTransaction &pending) {
      DoReads(pending.tx);
    });
  }

  void Terminate() override {
    bool already_terminated = m_stream_terminated.exchange(true);
    if (!already_terminated)
      m_stream.Terminate();

    m_endpoint.Terminate(
        [this](PendingTransaction &pending) { DoReads(pending.tx); });
  }

  void Join(const std::coroutine_handle<> &to_complete) override {
    return m_endpoint.Join(to_complete, [this](PendingTransaction &pending) {
      DoReads(pending.tx);
    });
  }

private:
  struct PendingTransaction {
    TxInProgress &tx;
  };

  using Endpoint = PushQueueEndpoint<PendingTransaction>;

  void DoReads(TxInProgress &tx) {
    for (auto act : tx.Incomplete()) {
      auto [ract, rout] = act.As<ReadBytes>();
      if (nullptr == ract) {
        tx.Done(act);
        continue;
      }

      std::span<std::byte> dest(reinterpret_cast<std::byte *>(rout->Data()),
                                ract->size);

      auto [num_read, error] = m_stream.Read(dest);
      rout->num_read = num_read;

      if (error) {
        tx.Fail<Cause>(error.GetCause());
        break;
      }

      tx.Done(act);
    }
  }

  CheckedTask<Status> Execute(TxInProgress &&tx) override {
    size_t total_bytes_to_write = 0;
    std::optional<TxInProgress::ActionOutVariant> first_write;

    for (auto act : tx.Incomplete()) {
      auto [wact, wout] = act.As<WriteBytes>();
      if (nullptr == wact)
        continue;

      total_bytes_to_write += wact->size;

      if (!first_write.has_value())
        first_write = act;
    }

    auto enqueuer = m_endpoint.LockForEnqueue();

    if (first_write.has_value()) {
      auto [first_wact, _] = first_write->As<WriteBytes>();

      if (first_wact->size == total_bytes_to_write) {
        if (!WriteOne(tx, *first_write))
          co_return tx.Finish();
      } else {
        if (!WriteMany(tx, *first_write, total_bytes_to_write))
          co_return tx.Finish();
      }
    }

    auto incomplete = tx.Incomplete();
    if (incomplete.begin() == incomplete.end())
      co_return tx.Finish();

    Endpoint::Item item(tx);
    auto enqueued_reads = enqueuer.Enqueue(item);
    co_await enqueued_reads;

    co_return tx.Finish();
  }

  bool WriteOne(TxInProgress &tx, TxInProgress::ActionOutVariant &first_write) {
    auto [first_wact, first_wout] = first_write.As<WriteBytes>();

    first_wout->num_written = 0;

    auto to_write = first_wact->Span();

    while (!to_write.empty()) {
      auto [num_written, error] = m_stream.Write(to_write);

      num_written = std::min<size_t>(num_written, to_write.size());
      first_wout->num_written += num_written;
      to_write = to_write.subspan(num_written);

      if (error) {
        tx.FailAt<Cause>(first_write, error.GetCause());
        return false;
      }
    }

    return true;
  }

  bool WriteMany(TxInProgress &tx, TxInProgress::ActionOutVariant &first_write,
                 size_t total_bytes_to_write) {
    m_write_buffer.Clear();

    auto [_, combined_data_ptr] = m_write_buffer.Allocate(total_bytes_to_write);
    if (nullptr == combined_data_ptr) {
      first_write.Out<WriteBytes>()->num_written = 0;
      tx.FailAt<CauseAllocationFailure>(first_write);
      return false;
    }

    std::span<std::byte> combined_data(
        reinterpret_cast<std::byte *>(combined_data_ptr), total_bytes_to_write);

    auto remaining_to_fill = combined_data;

    for (auto act : tx.Incomplete()) {
      auto [wact, wout] = act.As<WriteBytes>();
      if (nullptr == wact)
        continue;

      memcpy(remaining_to_fill.data(), wact->Span().data(), wact->size);
      remaining_to_fill = remaining_to_fill.subspan(wact->size);

      wout->num_written = 0;
    }

    auto remaining_to_write = combined_data;
    bool continuous_writes = true;

    while (!remaining_to_write.empty()) {
      auto [num_written, error] = m_stream.Write(remaining_to_write);
      size_t remaining_to_acknowledge =
          std::min<size_t>(num_written, remaining_to_write.size());

      remaining_to_write = remaining_to_write.subspan(remaining_to_acknowledge);

      for (auto act : tx.Incomplete()) {
        auto [wact, wout] = act.As<WriteBytes>();
        if (nullptr == wact) {
          continuous_writes = false;
          continue;
        }

        size_t to_acknowledge = std::min<size_t>(
            remaining_to_acknowledge, wact->size - wout->num_written);
        wout->num_written += to_acknowledge;
        remaining_to_acknowledge -= to_acknowledge;

        bool last = 0 == remaining_to_acknowledge;

        if (last && error) {
          tx.FailAt<Cause>(act, error.GetCause());
          return false;
        }

        if (continuous_writes && wout->num_written == wact->size)
          tx.Done(act);

        if (last)
          break;
      }
    }

    return true;
  }

  BlockingByteStream &m_stream;
  std::atomic_bool m_stream_terminated = false;

  Endpoint m_endpoint;
  TBuffer m_write_buffer;
};

} // namespace edr

#endif
