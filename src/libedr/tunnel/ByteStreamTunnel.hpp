#ifndef LIBEDR_TUNNEL_BYTESTREAMTUNNEL_HPP
#define LIBEDR_TUNNEL_BYTESTREAMTUNNEL_HPP

#include "libedr/driver/AnyAction.hpp"
#include "libedr/driver/Driver.hpp"
#include "libedr/driver/Error.hpp"
#include "libedr/driver/Logger.hpp"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/driver/bytestream/ByteStream.hpp"
#include "libedr/driver/bytestream/ByteStreamAction.hpp"
#include "libedr/util/adt/BucketArrayMap.hpp"
#include "libedr/util/adt/BucketArrayStack.hpp"
#include "libedr/util/asynchronicity/Asynchronicity.hpp"
#include "libedr/util/asynchronicity/AsynchronousPrimitives.hpp"
#include "libedr/util/memory/FreeListAllocator.hpp"
#include "libedr/util/miscellaneous/Spinlock.hpp"

#include <limits>
#include <memory_resource>

namespace edr {

namespace {
template <class T> inline std::span<const std::byte> ToBytes(const T &obj) {
  return std::span<const std::byte>(reinterpret_cast<const std::byte *>(&obj),
                                    sizeof(T));
}

template <class T>
inline const T &FromBytes(const std::span<const std::byte> &bytes) {
  assert(bytes.size() >= sizeof(T));
  return *reinterpret_cast<const T *>(bytes.data());
}

inline std::span<const std::byte> StringToBytes(const std::string_view &view) {
  return std::span<const std::byte>(
      reinterpret_cast<const std::byte *>(view.data()), view.size());
}

inline std::string_view
StringFromBytes(const std::span<const std::byte> &bytes) {
  return std::string_view(reinterpret_cast<const char *>(bytes.data()),
                          bytes.size());
}
} // namespace

class ByteStreamTunnelResourceStorage {
public:
  ByteStreamTunnelResourceStorage(std::pmr::memory_resource &resource)
      : m_resource(resource) {}

protected:
  FreeListAllocatorResource m_resource;
};

class ByteStreamTunnel : public ByteStreamTunnelResourceStorage,
                         public Asynchronous<ByteFreeListAllocator>,
                         public Tunnel {
  using DriverIndex = uint32_t;
  using TransactionKey = uint32_t;
  using LengthType = uint32_t;

  static constexpr LengthType g_max_length =
      std::numeric_limits<LengthType>::max();

public:
  template <class TDriver> class TunnelledDriver final : public TDriver {
  public:
    TunnelledDriver(ByteStreamTunnel &tunnel, DriverIndex driver_index,
                    std::string_view name)
        : TDriver(tunnel.m_context, name), m_tunnel(tunnel),
          m_driver_index(driver_index) {}

    ~TunnelledDriver() override { Terminate(); }

    void Join(const std::coroutine_handle<> &to_complete) override {
      m_tunnel.Join(to_complete);
    }

    void Terminate() override { m_tunnel.Terminate(); }

  private:
    CheckedTask<typename TDriver::Status>
    Execute(TransactionInProgress<typename TDriver::Action> &&tx) override {
      size_t input_length = 0;
      TransactionStorage &storage = GetTransactionStorage(tx);

      storage.ins.ForEachChunk([&input_length](std::span<std::byte> &data) {
        input_length += data.size();
      });

      if (input_length > g_max_length) {
        tx.FailCauseStringMessage(
            "The length of transaction inputs ({} bytes) exceeds the maximum "
            "supported length ({} bytes)",
            input_length, g_max_length);
        co_return tx.Finish();
      }

      size_t output_length = 0;
      storage.outs.ForEachChunk([&output_length](std::span<std::byte> &data) {
        output_length += data.size();
      });

      if (output_length > g_max_length) {
        tx.FailCauseStringMessage(
            "The length of expected transaction results ({} bytes) exceeds the "
            "maximum supported length ({} bytes)",
            output_length, g_max_length);
        co_return tx.Finish();
      }

      TransactionKey key{};
      auto pending_awaitable = m_tunnel.ScheduleRemoteTransaction(tx, key);
      if (!pending_awaitable) {
        tx.FailCauseStringMessage("Failed to add the transaction to the list "
                                  "of pending transactions");
        co_return tx.Finish();
      }

      ByteStream::Builder xact =
          m_tunnel.m_bstream.Initiate(tx, "ForwardToRemote");

      xact.Add<WriteBytes>(ToBytes(Request));

      RequestHeader header{
          .driver_index = m_driver_index,
          .transaction_key = key,
          .input_length = static_cast<LengthType>(input_length),
          .output_length = static_cast<LengthType>(output_length),
      };

      xact.Add<WriteBytes>(ToBytes(header));

      storage.ins.ForEachChunk(
          [&xact](std::span<std::byte> &data) { xact.Add<WriteBytes>(data); });

      auto status = co_await m_tunnel.m_bstream.Schedule(std::move(xact));
      if (!status) {
        {
          SpinlockGuard lock(m_tunnel.m_spinlock);
          m_tunnel.m_pending_transactions.Erase(key);
          m_tunnel.m_free_keys.Emplace(key);
        }

        tx.template Fail<CauseNestedError>(status.GetError());
        co_return tx.Finish();
      }

      Error error = co_await pending_awaitable;
      if (error)
        tx.Fail(error.GetCause());

      co_return tx.Finish();
    }

    ByteStreamTunnel &m_tunnel;
    DriverIndex m_driver_index;
  };

  template <class... Args> using Task = Asynchronous::template Task<Args...>;

  explicit ByteStreamTunnel(const DriverContext &ctx, ByteStream &m_bstream)
      : ByteStreamTunnelResourceStorage(ctx.TaskFrameResource()),
        Asynchronous(ByteFreeListAllocator(m_resource)), m_context(ctx),
        m_bstream(m_bstream), m_my_drivers(ctx.TransactionBufferPMRAllocator()),
        m_driver_names(ctx.TransactionBufferResource()),
        m_remote_drivers(ctx.TransactionBufferPMRAllocator()),
        m_free_keys(ctx.TransactionBufferPMRAllocator()),
        m_pending_transactions(*this) {}

  ~ByteStreamTunnel() { Terminate(); }

  bool RegisterDriver(DriverBase &driver) {
    auto *emplaced = m_my_drivers.Emplace(m_next_my_driver_index, driver);
    if (nullptr == emplaced)
      return false;

    m_next_my_driver_index++;
    return true;
  }

  Task<Error> Handshake() {
    auto def_xact =
        m_bstream.Initiate("ByteStreamTunnel::Handshake--DefineMyDrivers");

    size_t num_drivers = m_next_my_driver_index;
    for (size_t i = 0; i < num_drivers; i++) {
      auto &driver = m_my_drivers.Get(i)->get();

      auto name_length =
          std::min<LengthType>(g_max_length, driver.GetName().size());

      def_xact.Add<WriteBytes>(ToBytes(DefineDriver));

      DefineDriverHeader header{.driver_id = driver.GetID(),
                                .driver_name_length = name_length};
      def_xact.Add<WriteBytes>(ToBytes(header));

      auto truncated_name = driver.GetName().substr(0, name_length);
      def_xact.Add<WriteBytes>(StringToBytes(truncated_name));
    }

    def_xact.Add<WriteBytes>(ToBytes(FinishedDefiningDrivers));

    auto status = co_await m_bstream.Schedule(std::move(def_xact));
    if (!status)
      co_return m_context.MakeError<CauseNestedError>(status.GetError());

    while (true) {
      status = co_await m_bstream.DoReuse<ReadBytes>(
          "ByteStreamTunnel::Handshake--ReadingRemoteDefinitions",
          std::move(status), sizeof(PacketID));
      if (!status)
        co_return m_context.MakeError<CauseNestedError>(status.GetError());

      PacketID id = FromBytes<PacketID>(status.Outs<ReadBytes>()->Span());
      if (FinishedDefiningDrivers == id) {
        EventLoop();
        co_return Error::Success();
      }

      if (DefineDriver == id) {
        status = co_await m_bstream.DoReuse<ReadBytes>(
            "ByteStreamTunnel::Handshake--ReadingDriverDefinitionHeader",
            std::move(status), sizeof(DefineDriverHeader));
        if (!status)
          co_return m_context.MakeError<CauseNestedError>(status.GetError());

        DefineDriverHeader header =
            FromBytes<DefineDriverHeader>(status.Outs<ReadBytes>()->Span());

        auto name_status = co_await m_bstream.DoReuse<ReadBytes>(
            "ByteStreamTunnel::Handshake--ReadingDriverName", std::move(status),
            header.driver_name_length);
        if (!name_status)
          co_return m_context.MakeError<CauseNestedError>(
              name_status.GetError());

        auto name = StringFromBytes(name_status.Outs<ReadBytes>()->Span());
        auto name_pos = vss::Emplace<vss::String>(m_driver_names, name);
        if (m_driver_names.HadAllocationFailure())
          co_return m_context.MakeError<CauseAllocationFailure>();

        auto name_stream = m_driver_names.From(name_pos);
        std::string_view saved_name_ref =
            vss::Extract<vss::String>(name_stream)->View();

        bool known = false;
        ActionByDriver::ForID(header.driver_id,
                              [&known]<DriverID, class>() { known = true; });

        if (!known)
          co_return m_context.MakeErrorCauseStringMessage(
              "Unrecognized remote driver ID {}", header.driver_id);

        auto *emplaced =
            m_remote_drivers.Emplace(m_next_remote_driver_index, known);
        if (nullptr == emplaced)
          co_return m_context.MakeError<CauseAllocationFailure>();

        ActionByDriver::ForID(
            header.driver_id,
            [this, &emplaced, &saved_name_ref]<DriverID t_id, class TAction>() {
              using TDriver = Driver<t_id, TAction>;
              static_assert(sizeof(TunnelledDriver<TDriver>) ==
                            sizeof(emplaced->instance_placeholder));
              new (&emplaced->instance_placeholder[0]) TunnelledDriver<TDriver>(
                  *this, m_next_remote_driver_index, saved_name_ref);
            });

        m_next_remote_driver_index++;
        continue;
      }

      co_return m_context.MakeErrorCauseStringMessage(
          "Unexpected incoming packet ID {}", static_cast<uint32_t>(id));
    }
  }

  template <class TDriver>
  TunnelledDriver<TDriver> *FindByName(std::string_view name) {
    DriverID id = TDriver::g_id;
    for (auto &remote_driver : m_remote_drivers)
      if (remote_driver.valid && remote_driver.Get().GetID() == id &&
          remote_driver.Get().GetName() == name)
        return static_cast<TunnelledDriver<TDriver> *>(&remote_driver.Get());

    return nullptr;
  }

  void Join(const std::coroutine_handle<> &to_complete) {
    m_bstream.Join(to_complete);
  }

  void Terminate() { m_bstream.Terminate(); }

private:
  enum PacketID : uint32_t {
    DefineDriver = 1,
    FinishedDefiningDrivers = 2,
    Request = 3,
    Response = 4,
  };

  struct DefineDriverHeader {
    DriverID driver_id;
    LengthType driver_name_length;
  };

  struct RequestHeader {
    DriverIndex driver_index;
    TransactionKey transaction_key;
    LengthType input_length;
    LengthType output_length;
  };

  struct ResponseHeader {
    TransactionKey transaction_key;
    LengthType first_incomplete_input_offset;
    LengthType first_incomplete_output_offset;
    LengthType error_length;
  };

  static_assert(sizeof(RequestHeader) == sizeof(ResponseHeader));

  struct alignas(TunnelledDriver<ByteStream>) RemoteDriver {
    RemoteDriver(bool valid) : valid(valid) {}

    bool valid = false;
    std::byte instance_placeholder[sizeof(TunnelledDriver<ByteStream>)];

    ~RemoteDriver() {
      if (valid)
        Get().~DriverBase();
    }

    DriverBase &Get() {
      assert(valid);
      return *reinterpret_cast<DriverBase *>(&instance_placeholder[0]);
    }
  };

  struct PendingTransaction {
    TransactionStorage &storage;
    std::optional<TBuffer> &error_buffer;
    TransactionStorage::ActionPosition &first_incomplete;
  };

  Task<> EventLoop() {
    auto status = m_bstream.MakeEmptyStatus();
    while (true) {
      auto total_size = sizeof(PacketID) + sizeof(RequestHeader);

      status = co_await m_bstream.DoReuse<ReadBytes>(
          "ByteStreamTunnel::EventLoop--ReadIncomingHeader", std::move(status),
          total_size);
      if (!status) {
        AbortAll<CauseNestedError>(status.GetError());
        co_return;
      }

      auto *header_out = status.Outs<ReadBytes>();

      auto packet_id = FromBytes<PacketID>(header_out->Span());
      const auto *header_ptr =
          reinterpret_cast<std::byte *>(header_out->Data()) + sizeof(PacketID);

      std::optional<Error> mb_error;

      if (Request == packet_id) {
        const auto &header =
            *reinterpret_cast<const RequestHeader *>(header_ptr);
        mb_error = co_await HandleRequest(header, status);
      } else if (Response == packet_id) {
        const auto &header =
            *reinterpret_cast<const ResponseHeader *>(header_ptr);
        mb_error = co_await HandleResponse(header, status);
      } else
        mb_error = m_context.MakeErrorCauseStringMessage(
            "Unexpected packet ID {}", static_cast<uint32_t>(packet_id));

      auto &error = *mb_error;
      if (error) {
        AbortAll(error);
        co_return;
      }
    }
  }

  Task<Error> HandleRequest(RequestHeader header, ByteStream::Status &status) {
    status = co_await m_bstream.DoReuse<ReadBytes>(
        "ByteStreamTunnel::HandleRequest--ReadInputs", std::move(status),
        header.input_length);
    if (!status)
      co_return m_context.MakeError<CauseNestedError>(status.GetError());

    auto inputs = status.Outs<ReadBytes>()->Span();

    auto *driver = m_my_drivers.Get(header.driver_index);
    if (nullptr == driver)
      co_return co_await AbortTransaction(header.transaction_key, status);

    std::optional<Task<Error>> initiator;

    ActionByDriver::ForID(
        driver->get().GetID(), [&]<DriverID t_driver_id, IsAction TAction>() {
          initiator.emplace(ForwardRequest<t_driver_id, TAction>(
              static_cast<Driver<t_driver_id, TAction> &>(driver->get()),
              header, inputs, status));
        });

    assert(initiator);
    co_return co_await *initiator;
  }

  template <DriverID t_driver_id, IsAction TAction>
  Task<Error> ForwardRequest(Driver<t_driver_id, TAction> &driver,
                             RequestHeader header, std::span<std::byte> inputs,
                             ByteStream::Status &status) {
    TransactionBuilder<TAction> builder =
        driver.Initiate("IncomingRemoteTransaction");
    TransactionStorage &storage = GetTransactionStorage(builder);

    auto [_, input_ptr] = storage.ins.Allocate(inputs.size());
    auto [__, output_ptr] = storage.outs.Allocate(header.output_length);

    if (nullptr != input_ptr)
      memcpy(input_ptr, inputs.data(), inputs.size());

    auto detached =
        DetachRequest(driver, header.transaction_key, std::move(builder));
    if (!detached)
      co_return co_await AbortTransaction(header.transaction_key, status);

    co_return Error::Success();
  }

  template <DriverID t_driver_id, IsAction TAction>
  Task<> DetachRequest(Driver<t_driver_id, TAction> &driver, TransactionKey key,
                       TransactionBuilder<TAction> &&builder) {
    auto status = co_await driver.Schedule(std::move(builder));

    TransactionStorage &storage = GetTransactionStorage(status);
    auto &first_incomplete = GetPositionOfFirstIncomplete(status);

    size_t input_offset = storage.ins.Distance(first_incomplete.first);
    size_t output_offset = storage.outs.Distance(first_incomplete.second);

    if (input_offset > g_max_length || output_offset > g_max_length) {
      input_offset = 0;
      output_offset = 0;
    }

    size_t error_length = 0;

    auto &error_buffer = GetErrorBuffer(status);
    if (error_buffer)
      error_buffer->ForEachChunk([&error_length](std::span<std::byte> bytes) {
        error_length += bytes.size();
      });

    if (error_length > g_max_length)
      error_length = 0;

    ResponseHeader header{
        .transaction_key = key,
        .first_incomplete_input_offset = static_cast<LengthType>(input_offset),
        .first_incomplete_output_offset =
            static_cast<LengthType>(output_offset),
        .error_length = static_cast<LengthType>(error_length),
    };

    auto response_xact =
        m_bstream.Initiate("ByteStreamTunnel::DetachRequest--SendResponse");

    response_xact.Add<WriteBytes>(ToBytes(Response));
    response_xact.Add<WriteBytes>(ToBytes(header));

    size_t output_left = output_offset;
    storage.outs.ForEachChunk(
        [&response_xact, &output_left](std::span<std::byte> bytes) {
          size_t to_write = std::min<size_t>(bytes.size(), output_left);
          if (0 != to_write) {
            response_xact.Add<WriteBytes>(bytes.subspan(0, to_write));
            output_left -= to_write;
          }
        });

    if (0 != error_length)
      error_buffer->ForEachChunk([&response_xact](std::span<std::byte> bytes) {
        response_xact.Add<WriteBytes>(bytes);
      });

    m_bstream.Schedule(std::move(response_xact));
  }

  Task<Error> HandleResponse(ResponseHeader header,
                             ByteStream::Status &status) {
    bool has_output = 0 != header.first_incomplete_output_offset;
    bool has_error = 0 != header.error_length;

    auto *pending = m_pending_transactions.Get(header.transaction_key);
    if (nullptr != pending) {
      auto incomplete_input = pending->data.storage.ins.FromDistance(
          header.first_incomplete_input_offset);
      auto incomplete_output = pending->data.storage.outs.FromDistance(
          header.first_incomplete_output_offset);

      pending->data.first_incomplete = {incomplete_input, incomplete_output};
    }

    if (has_output || has_error) {
      auto xact = m_bstream.Initiate(
          "ByteStreamTunnel::HandleResponse--ReceiveOutput", std::move(status));

      if (has_output)
        xact.Add<ReadBytes>(header.first_incomplete_output_offset);

      if (has_error)
        xact.Add<ReadBytes>(header.error_length);

      status = co_await m_bstream.Schedule(std::move(xact));
      if (!status)
        co_return m_context.MakeError<CauseNestedError>(status.GetError());

      if (nullptr == pending)
        co_return Error::Success();

      auto it = status.begin();

      if (has_output) {
        std::span<std::byte> output = (it++)->Out<ReadBytes>()->Span();
        pending->data.storage.outs.ForEachChunk(
            [&output](std::span<std::byte> &dest) {
              size_t to_copy = std::min(dest.size(), output.size());
              if (0 != to_copy) {
                memcpy(dest.data(), output.data(), to_copy);
                output = output.subspan(to_copy);
              }
            });
      }

      if (has_error) {
        std::span<std::byte> error = (it++)->Out<ReadBytes>()->Span();

        auto &mem_resource = pending->data.storage.ins.GetMemoryResource();
        pending->data.error_buffer.emplace(
            pending->data.storage.owner->TakeOrMake(mem_resource));

        auto [_, dest] = pending->data.error_buffer->Allocate(error.size());
        if (nullptr == dest)
          pending->data.error_buffer.reset();
        else
          memcpy(dest, error.data(), error.size());
      }
    }

    if (nullptr == pending)
      co_return Error::Success();

    pending->resolver.return_value(Error::Success());

    m_pending_transactions.Erase(header.transaction_key);

    {
      SpinlockGuard lock(m_spinlock);
      m_free_keys.Emplace(header.transaction_key);
    }

    co_return Error::Success();
  }

  template <class TAction>
  auto ScheduleRemoteTransaction(TransactionInProgress<TAction> &tx,
                                 TransactionKey &key) {
    SpinlockGuard lock(m_spinlock);

    if (!m_free_keys.Empty())
      key = m_free_keys.Pop();
    else
      key = m_next_key++;

    return m_pending_transactions.Emplace(
        key, PendingTransaction{.storage = GetTransactionStorage(tx),
                                .error_buffer = GetErrorBuffer(tx),
                                .first_incomplete =
                                    GetPositionOfFirstIncomplete(tx)});
  }

  template <class TCause, class... Args> void AbortAll(Args &&...args) {
    SpinlockGuard lock(m_spinlock);
    for (auto &pending : m_pending_transactions)
      pending.resolver.return_value(
          m_context.MakeError<TCause>(std::forward<Args>(args)...));
  }

  void AbortAll(Error &error) {
    SpinlockGuard lock(m_spinlock);
    for (auto &pending : m_pending_transactions)
      pending.resolver.return_value(
          Error::Copy(m_context.TransactionBufferResource(), error));
  }

  Task<Error> AbortTransaction(TransactionKey key, ByteStream::Status &status) {
    ResponseHeader response{
        .transaction_key = key,
        .first_incomplete_input_offset = 0,
        .first_incomplete_output_offset = 0,
        .error_length = 0,
    };

    status = co_await m_bstream.DoReuse<WriteBytes>(
        "ByteStreamTunnel::HandleRequest--AbortingTransaction",
        std::move(status), ToBytes(response));
    if (!status)
      co_return m_context.MakeError<CauseNestedError>(status.GetError());

    co_return Error::Success();
  }

  using MyDriverMap =
      BucketArrayMap<std::reference_wrapper<DriverBase>, 8,
                     std::pmr::polymorphic_allocator<std::byte>>;

  using RemoteDriverMap =
      BucketArrayMap<RemoteDriver, 8,
                     std::pmr::polymorphic_allocator<std::byte>>;

  using FreeKeysStack =
      BucketArrayStack<TransactionKey, 32,
                       std::pmr::polymorphic_allocator<std::byte>>;

  using PendingTransactionsMap =
      ResolutionMap<ByteStreamTunnel, 8, PendingTransaction, Error>;

  DriverContext m_context;

  ByteStream &m_bstream;

  MyDriverMap m_my_drivers;
  DriverIndex m_next_my_driver_index = 0;

  TransactionBuffer<1024> m_driver_names;
  RemoteDriverMap m_remote_drivers;
  DriverIndex m_next_remote_driver_index = 0;

  Spinlock m_spinlock;
  FreeKeysStack m_free_keys;
  TransactionKey m_next_key = 0;

  PendingTransactionsMap m_pending_transactions;
};

} // namespace edr

#endif
