#ifndef LIBEDR_TUNNEL_BYTESTREAMTUNNEL_HPP
#define LIBEDR_TUNNEL_BYTESTREAMTUNNEL_HPP

#include "libedr/driver/AnyAction.hpp"
#include "libedr/driver/Driver.hpp"
#include "libedr/driver/Error.hpp"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/driver/bytestream/ByteStream.hpp"
#include "libedr/driver/bytestream/ByteStreamAction.hpp"
#include "libedr/util/adt/BucketArrayMap.hpp"
#include "libedr/util/adt/BucketArrayStack.hpp"
#include "libedr/util/asynchronicity/Asynchronicity.hpp"
#include "libedr/util/asynchronicity/ResolutionMap.hpp"
#include "libedr/util/memory/FreeListAllocator.hpp"
#include "libedr/util/miscellaneous/ScopeGuard.hpp"
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
  template <class TDriver> class TunneledDriver final : public TDriver {
  public:
    TunneledDriver(ByteStreamTunnel &tunnel, DriverIndex driver_index,
                   std::string_view name)
        : TDriver(tunnel.m_context, name), m_tunnel(tunnel),
          m_driver_index(driver_index) {}

    ~TunneledDriver() override { Terminate(); }

    bool Serve(bool wait_if_empty) override {
      return m_tunnel.Serve(wait_if_empty);
    }

    void Join(const std::coroutine_handle<> &to_complete) override {
      m_tunnel.Join(to_complete);
    }

    void Terminate() override { m_tunnel.Terminate(); }

  private:
    CheckedTask<typename TDriver::Status>
    Execute(TransactionInProgress<typename TDriver::Action> &&tx) override;

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

  bool IsAlive() { return m_is_alive.load(); }

  bool RegisterDriver(DriverBase &driver);

  Task<Error> Handshake();

  template <class TDriver>
  TunneledDriver<TDriver> *FindByName(std::string_view name);

  bool Serve(bool wait_if_empty) { return m_bstream.Serve(wait_if_empty); }

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

  struct alignas(TunneledDriver<ByteStream>) RemoteDriver {
    RemoteDriver(bool valid) : valid(valid) {}

    bool valid = false;
    std::byte instance_placeholder[sizeof(TunneledDriver<ByteStream>)];

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

  Task<> EventLoop();

  Task<Error> HandleRequest(RequestHeader header, ByteStream::Status &status);

  template <DriverID t_driver_id, IsAction TAction>
  Task<Error> ForwardRequest(Driver<t_driver_id, TAction> &driver,
                             RequestHeader header, std::span<std::byte> inputs,
                             ByteStream::Status &status);

  template <DriverID t_driver_id, IsAction TAction>
  Task<> DetachRequest(Driver<t_driver_id, TAction> &driver, TransactionKey key,
                       TransactionBuilder<TAction> &&builder);

  Task<Error> HandleResponse(ResponseHeader header, ByteStream::Status &status);

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

  void WakeUpExecutionGates() {
    size_t num_drivers = m_next_my_driver_index;
    for (size_t i = 0; i < num_drivers; i++) {
      auto &driver = m_my_drivers.Get(i)->get();
      if (driver.GetID() == DriverID::ExecutionGate)
        driver.Terminate();
    }
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

  std::atomic_bool m_is_alive = false;
};

} // namespace edr

#endif
