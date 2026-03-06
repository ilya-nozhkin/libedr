#ifndef LIBEDR_DRIVER_DRIVER_HPP
#define LIBEDR_DRIVER_DRIVER_HPP

#include "libedr/driver/Action.hpp"
#include "libedr/driver/Error.hpp"
#include "libedr/driver/Logger.hpp"
#include "libedr/driver/TransactionBuffer.hpp"
#include "libedr/driver/UniqueIDs.hpp"
#include "libedr/util/adt/FreeList.hpp"
#include "libedr/util/asynchronicity/Asynchronicity.hpp"
#include "libedr/util/memory/FreeListAllocator.hpp"
#include "libedr/util/miscellaneous/Spinlock.hpp"
#include "libedr/util/vss/VSS.hpp"

#include <coroutine>
#include <format>
#include <memory_resource>
#include <optional>
#include <utility>

namespace edr {

static inline constexpr size_t g_min_transaction_data_chunk_size = 1024;

namespace {

using TBuffer = TransactionBuffer<g_min_transaction_data_chunk_size>;

} // namespace

struct DependentTransactions {
  const DependentTransactions *prev = nullptr;
  size_t length = 0;

  std::string_view driver_name;
  std::string_view tx_description;
};

} // namespace edr

template <> struct std::formatter<edr::DependentTransactions, char> {
  bool is_indentation = false;
  size_t additional_indentation = 0;

  template <class ParseContext>
  constexpr ParseContext::iterator parse(ParseContext &ctx) {
    auto it = ctx.begin();
    while (*it != '}') {
      if (*it == 'i')
        is_indentation = true;

      if (*it >= '0' && *it <= '9')
        additional_indentation = *it - '0';

      it++;
    }

    return it;
  }

  template <class FmtContext>
  FmtContext::iterator format(const edr::DependentTransactions &deps,
                              FmtContext &ctx) const {
    if (is_indentation) {
      auto it = ctx.out();
      for (size_t i = 0; i < deps.length + additional_indentation; i++)
        it = std::format_to(it, "| ");

      return it;
    }

    return (nullptr == deps.prev)
               ? std::format_to(ctx.out(), "({}) {}", deps.driver_name,
                                deps.tx_description)
               : std::format_to(ctx.out(), "{} -> ({}) {}", *deps.prev,
                                deps.driver_name, deps.tx_description);
  }
};

namespace edr {

struct TransactionStorage final {
  using ActionPosition = std::pair<TBuffer::Position, TBuffer::Position>;

  TransactionStorage(Spinlock &owner_spinlock, FreeList<TBuffer> &owner,
                     TBuffer &&ins, TBuffer &&outs)
      : owner(&owner), ins(std::move(ins)), outs(std::move(outs)),
        m_owner_spinlock(&owner_spinlock) {}

  TransactionStorage(TransactionStorage &&from)
      : owner(from.owner), ins(std::move(from.ins)), outs(std::move(from.outs)),
        m_owner_spinlock(from.m_owner_spinlock) {
    from.owner = nullptr;
  }

  TransactionStorage &operator=(TransactionStorage &&from) {
    FreeResources();

    m_owner_spinlock = from.m_owner_spinlock;
    owner = from.owner;
    ins = std::move(from.ins);
    outs = std::move(from.outs);

    from.owner = nullptr;
    return *this;
  }

  ~TransactionStorage() { FreeResources(); }

  TransactionStorage(const TransactionStorage &) = delete;
  TransactionStorage &operator=(const TransactionStorage &) = delete;

  FreeList<TBuffer> *owner;
  TBuffer ins;
  TBuffer outs;

private:
  Spinlock *m_owner_spinlock;

  void FreeResources() {
    if (nullptr != owner) {
      SpinlockGuard lock(*m_owner_spinlock);

      if (!ins.HadAllocationFailure()) {
        ins.Clear();
        owner->Free(std::move(ins));
      }

      if (!outs.HadAllocationFailure()) {
        outs.Clear();
        owner->Free(std::move(outs));
      }

      owner = nullptr;
    }
  }
};

template <class T> struct TypedActionPosition {
  TransactionStorage::ActionPosition position;
};

template <IsAction TAction> class TransactionStatus;

template <IsAction TAction> class TransactionBuilder {
  friend class TransactionStatus<TAction>;
  friend class Tunnel;

public:
  TransactionBuilder(TransactionStorage &&storage,
                     const DependentTransactions &dependent)
      : m_storage(std::move(storage)), m_dependent(dependent) {}

  template <class T, class... Args> TypedActionPosition<T> Add(Args &&...args) {
    auto in_pos = vss::Emplace<typename TAction::template Option<T>>(
        m_storage.ins, std::forward<Args>(args)...);
    auto out_pos = vss::Emplace<typename T::Out>(m_storage.outs,
                                                 std::forward<Args>(args)...);

    return TypedActionPosition<T>{{in_pos, out_pos}};
  }

  bool HadAllocationFailure() {
    return m_storage.ins.HadAllocationFailure() ||
           m_storage.outs.HadAllocationFailure();
  }

  const DependentTransactions &GetDependent() const { return m_dependent; }

private:
  TransactionStorage m_storage;
  DependentTransactions m_dependent;
};

template <IsAction TAction> class TransactionStatus {
  template <IsAction TTAction> friend class TransactionInProgress;
  template <DriverID t_driver_id, IsAction TTAction> friend class Driver;
  friend class Tunnel;

public:
  using ActionPosition = TransactionStorage::ActionPosition;

  TransactionStatus(TransactionBuilder<TAction> &&builder)
      : m_storage(std::move(builder.m_storage)),
        m_dependent(builder.m_dependent),
        m_first_incomplete(m_storage.ins.Begin(), m_storage.outs.Begin()) {}

  TransactionStatus(TransactionStatus &&from)
      : m_storage(std::move(from.m_storage)), m_dependent(from.m_dependent),
        m_first_incomplete(std::move(from.m_first_incomplete)),
        m_error_buffer(std::move(from.m_error_buffer)) {
    from.m_error_buffer.reset();
  }

  TransactionStatus &operator=(TransactionStatus &&from) {
    if (m_error_buffer) {
      m_error_buffer->Clear();
      m_storage.owner->Free(std::move(*m_error_buffer));
      m_error_buffer.reset();
    }

    m_storage = std::move(from.m_storage);
    m_dependent = from.m_dependent;
    m_first_incomplete = from.m_first_incomplete;
    m_error_buffer = std::move(from.m_error_buffer);

    from.m_error_buffer.reset();
    return *this;
  }

  TransactionStatus(const TransactionStatus &) = delete;
  TransactionStatus &operator=(const TransactionStatus &) = delete;

  ~TransactionStatus() {
    if (m_error_buffer) {
      m_error_buffer->Clear();
      m_storage.owner->Free(std::move(*m_error_buffer));
      m_error_buffer.reset();
    }
  }

  class ActionOutVariant {
    friend class TransactionStatus;
    template <IsAction TTAction> friend class TransactionInProgress;

  public:
    ActionOutVariant(ActionPosition pos, TAction *action, void *out)
        : m_pos(pos), m_action(action), m_out(out) {}

    operator bool() {
      return nullptr != m_action && nullptr != m_out &&
             static_cast<bool>(m_action);
    }

    std::optional<ActionID> GetID() {
      if (nullptr == m_action)
        return std::nullopt;

      return m_action->discriminant;
    }

    template <class F> bool Visit(F &&func) {
      if (nullptr == m_action || nullptr == m_out)
        return false;

      return m_action->Visit([&]<class A>(A &action) {
        func(action, *reinterpret_cast<A::Out *>(m_out));
      });
    }

    template <class T> std::pair<T *, typename T::Out *> As() {
      if (nullptr == m_action || nullptr == m_out)
        return {nullptr, nullptr};

      T *action_ptr = m_action->template As<T>();
      if (nullptr == action_ptr)
        return {nullptr, nullptr};

      return {action_ptr, reinterpret_cast<T::Out *>(m_out)};
    }

    template <class T> typename T::Out *Out() {
      auto [_, out] = As<T>();
      return out;
    }

    ActionOutVariant *operator->() { return this; }

  private:
    ActionPosition m_pos;
    TAction *m_action;
    void *m_out;
  };

  class Iterator {
    friend class TransactionStatus;
    template <IsAction TTAction> friend class TransactionInProgress;

  public:
    Iterator(TBuffer &ins, TBuffer &outs, const ActionPosition &pos)
        : m_ins(ins), m_outs(outs), m_pos(pos) {}

    ActionOutVariant operator*() const {
      auto in_is = m_ins.From(m_pos.first);
      auto out_is = m_outs.From(m_pos.second);

      TAction *action_ptr = vss::Extract<TAction>(in_is);
      if (nullptr == action_ptr)
        return ActionOutVariant(m_pos, nullptr, nullptr);

      void *out_ptr = nullptr;
      action_ptr->Visit([&]<class A>(A &action) {
        out_ptr = vss::Extract<typename A::Out>(out_is, action);
      });

      return ActionOutVariant(m_pos, action_ptr, out_ptr);
    }

    ActionOutVariant operator->() const { return operator*(); }

    Iterator &operator++() {
      auto in_is = m_ins.From(m_pos.first);
      auto out_is = m_outs.From(m_pos.second);

      auto *action = vss::Extract<TAction>(in_is);
      if (nullptr == action) {
        m_pos = {m_ins.End(), m_outs.End()};
        return *this;
      }

      bool recognized = action->Visit([&]<class A>(A &action) {
        vss::Extract<typename A::Out>(out_is, action);
      });

      if (recognized)
        m_pos = {in_is.Tell(), out_is.Tell()};
      else
        m_pos = {m_ins.End(), m_outs.End()};

      return *this;
    }

    Iterator operator++(int) {
      auto tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(const Iterator &it) const {
      return m_pos.first == it.m_pos.first;
    }

  private:
    TBuffer &m_ins;
    TBuffer &m_outs;

    ActionPosition m_pos;
  };

  class ActionRange {
  public:
    ActionRange(TransactionStorage &storage, ActionPosition begin,
                ActionPosition end)
        : m_storage(storage), m_begin(begin), m_end(end) {}

    Iterator begin() {
      return Iterator(m_storage.ins, m_storage.outs, m_begin);
    }

    Iterator end() { return Iterator(m_storage.ins, m_storage.outs, m_end); }

  private:
    TransactionStorage &m_storage;
    ActionPosition m_begin;
    ActionPosition m_end;
  };

  const DependentTransactions &GetDependent() const { return m_dependent; }

  Iterator begin() {
    return Iterator(m_storage.ins, m_storage.outs,
                    {m_storage.ins.Begin(), m_storage.outs.Begin()});
  }

  Iterator end() {
    return Iterator(m_storage.ins, m_storage.outs,
                    {m_storage.ins.End(), m_storage.outs.End()});
  }

  ActionRange Incomplete() {
    return ActionRange(m_storage, m_first_incomplete,
                       {m_storage.ins.End(), m_storage.outs.End()});
  }

  ActionRange Complete() {
    return ActionRange(m_storage,
                       {m_storage.ins.Begin(), m_storage.outs.Begin()},
                       m_first_incomplete);
  }

  template <ActionType T>
  std::pair<T *, typename T::Out *> As(const TypedActionPosition<T> &pos) {
    Iterator it(m_storage.ins, m_storage.outs, pos.position);
    return it->template As<T>();
  }

  template <ActionType T>
  typename T::Out *Out(const TypedActionPosition<T> &pos) {
    Iterator it(m_storage.ins, m_storage.outs, pos.position);
    return it->template Out<T>();
  }

  template <ActionType... Ts> auto Outs() {
    Iterator it = begin();
    auto end_it = end();

    if constexpr (1 == sizeof...(Ts))
      return it->template Out<Ts...>();
    else
      return std::make_tuple([&]() -> Ts::Out * {
        if (it == end_it)
          return nullptr;

        return (it++)->template Out<Ts>();
      }()...);
  }

  ActionError *GetError() {
    if (!m_error_buffer)
      return nullptr;

    auto error_is = m_error_buffer->From(m_error_buffer->Begin());
    return vss::Extract<ActionError>(error_is);
  }

  AnyAction *FailedAction() {
    auto *error = GetError();
    if (nullptr == error)
      return nullptr;

    return error->FailedAction();
  }

  std::optional<ActionID> FailedActionID() {
    auto *error = GetError();
    if (nullptr == error)
      return std::nullopt;

    return error->FailedActionID();
  }

  Cause *GetCause() {
    auto *error = GetError();
    if (nullptr == error)
      return nullptr;

    return &error->GetCause();
  }

  template <class T> T *CauseAs() {
    auto *cause = GetCause();
    if (nullptr == cause)
      return nullptr;

    return cause->template As<T>();
  }

  operator bool() const {
    return !m_storage.ins.HadAllocationFailure() &&
           !m_storage.outs.HadAllocationFailure() &&
           m_first_incomplete.first == m_storage.ins.End();
  }

private:
  TransactionStorage m_storage;
  DependentTransactions m_dependent;

  ActionPosition m_first_incomplete;

  std::optional<TBuffer> m_error_buffer;
};

template <IsAction TAction> class TransactionInProgress final {
  friend class Tunnel;

public:
  TransactionInProgress(TransactionStatus<TAction> &&status)
      : m_status(std::move(status)) {}

  using ActionOutVariant = TransactionStatus<TAction>::ActionOutVariant;
  using Iterator = TransactionStatus<TAction>::Iterator;
  using ActionRange = TransactionStatus<TAction>::ActionRange;

  const DependentTransactions &GetDependent() const {
    return m_status.GetDependent();
  }

  ActionRange Incomplete() { return m_status.Incomplete(); }

  ActionRange Complete() { return m_status.Complete(); }

  void Done(const ActionOutVariant &action_out) {
    auto next_it = Iterator(m_status.m_storage.ins, m_status.m_storage.outs,
                            action_out.m_pos);
    next_it++;
    m_status.m_first_incomplete = next_it.m_pos;
  }

  void Done(const Iterator &it) {
    auto next_it = it;
    next_it++;
    m_status.m_first_incomplete = next_it.m_pos;
  }

  template <class TCause, class... Args> void Fail(Args &&...args) {
    if (m_status)
      return;

    FailImpl<TCause>(m_status.m_first_incomplete.first,
                     std::forward<Args>(args)...);
  }

  template <class... Args>
  void FailCauseStringMessage(std::format_string<Args...> format,
                              Args &&...args) {
    if (m_status)
      return;

    FailImpl<CauseStringMessage>(m_status.m_first_incomplete.first, format,
                                 std::forward<Args>(args)...);
  }

  void Fail(Cause *cause) {
    if (m_status || nullptr == cause)
      return;

    FailImpl<Cause>(m_status.m_first_incomplete.first, cause);
  }

  template <class TCause, class... Args>
  void FailAt(const ActionOutVariant &action_out, Args &&...args) {
    FailImpl<TCause>(action_out.m_pos.first, std::forward<Args>(args)...);
  }

  template <class TCause, class... Args>
  void FailAt(const Iterator &it, Args &&...args) {
    FailImpl<TCause>(it.m_pos.first, std::forward<Args>(args)...);
  }

  template <class... Args>
  void FailAtCauseStringMessage(const ActionOutVariant &action_out,
                                std::format_string<Args...> format,
                                Args &&...args) {
    FailImpl<CauseStringMessage>(action_out.m_pos.first, format,
                                 std::forward<Args>(args)...);
  }

  template <class... Args>
  void FailAtCauseStringMessage(const Iterator &it,
                                std::format_string<Args...> format,
                                Args &&...args) {
    FailImpl<CauseStringMessage>(it.m_pos.first, format,
                                 std::forward<Args>(args)...);
  }

  TransactionStatus<TAction> Finish() { return std::move(m_status); }

private:
  template <class TCause, class... Args>
  void FailImpl(const TBuffer::Position &in_pos, Args &&...args) {
    auto &mem_resource = m_status.m_storage.ins.GetMemoryResource();
    m_status.m_error_buffer.emplace(
        m_status.m_storage.owner->TakeOrMake(mem_resource));

    auto in_is = m_status.m_storage.ins.From(in_pos);
    auto *failed_action = vss::Extract<TAction>(in_is);

    if constexpr (std::is_same_v<TCause, Cause>) {
      Cause *cause_ptr = (args, ...);
      if (nullptr != cause_ptr)
        vss::Emplace<ActionError>(*m_status.m_error_buffer, failed_action,
                                  *cause_ptr);
    } else
      vss::Emplace<ActionError::Option<TCause>>(
          *m_status.m_error_buffer, failed_action, std::forward<Args>(args)...);

    if (m_status.m_error_buffer->HadAllocationFailure())
      m_status.m_error_buffer.reset();
  }

  TransactionStatus<TAction> m_status;
};

struct DriverContext {
  Logger &logger;
  std::pmr::memory_resource *transaction_buffer_resource = nullptr;
  std::pmr::memory_resource *task_frame_resource = nullptr;

  std::pmr::memory_resource &TransactionBufferResource() const {
    return ResourceOrDefault(transaction_buffer_resource);
  }

  auto TransactionBufferPMRAllocator() const {
    return std::pmr::polymorphic_allocator<std::byte>(
        &TransactionBufferResource());
  }

  std::pmr::memory_resource &TaskFrameResource() const {
    return ResourceOrDefault(task_frame_resource);
  }

  auto TaskFramePMRAllocator() const {
    return std::pmr::polymorphic_allocator<std::byte>(&TaskFrameResource());
  }

  template <class TCause, class... Args> Error MakeError(Args &&...args) const {
    return Error::Make<TCause>(TransactionBufferResource(),
                               std::forward<Args>(args)...);
  }

  template <class... Args>
  Error MakeErrorCauseStringMessage(std::format_string<Args...> format,
                                    Args &&...args) const {
    return Error::Make<CauseStringMessage>(TransactionBufferResource(), format,
                                           std::forward<Args>(args)...);
  }

private:
  static std::pmr::memory_resource &
  ResourceOrDefault(std::pmr::memory_resource *memory_resource) {
    return nullptr != memory_resource ? *memory_resource
                                      : *std::pmr::get_default_resource();
  }
};

class DriverBase {
public:
  DriverBase(std::pmr::memory_resource &resource) : m_resource(resource) {}

  virtual ~DriverBase() = default;

  virtual DriverID GetID() = 0;
  virtual std::string_view GetName() = 0;

protected:
  FreeListAllocatorResource m_resource;
};

template <DriverID t_driver_id, IsAction TAction>
class Driver : public DriverBase,
               public edr::Asynchronous<ByteFreeListAllocator> {
  using Asynchronous = edr::Asynchronous<ByteFreeListAllocator>;

  template <class Arg> struct ExpandArg {
    template <class F> static auto Do(F &&func, Arg &&arg) {
      return std::forward<F>(func)(std::forward<Arg>(arg));
    }
  };

  template <class... Args> struct ExpandArg<std::tuple<Args...>> {
    template <class F, class T> static auto Do(F &&func, T &&arg) {
      return std::apply(std::forward<F>(func), std::forward<T>(arg));
    }
  };

public:
  static inline constexpr auto g_id = t_driver_id;

  using Action = TAction;
  using Builder = TransactionBuilder<TAction>;
  using Status = TransactionStatus<TAction>;
  using TxInProgress = TransactionInProgress<TAction>;

  template <class... Args> using Task = Asynchronous::template Task<Args...>;
  template <class... Args>
  using CheckedTask = Asynchronous::template CheckedTask<Args...>;

  Driver(const DriverContext &ctx, std::string_view name)
      : DriverBase(ctx.TaskFrameResource()), Asynchronous(m_resource),
        m_memory_resource(ctx.TransactionBufferResource()),
        m_logger(ctx.logger), m_name(name), m_buffers(m_memory_resource) {}

  DriverID GetID() override { return g_id; }

  std::string_view GetName() override { return m_name; }

  Status MakeEmptyStatus() {
    DependentTransactions including_self{.prev = nullptr,
                                         .length = 0,
                                         .driver_name = m_name,
                                         .tx_description = ""};

    SpinlockGuard lock(m_spinlock);
    auto builder =
        Builder(TransactionStorage(m_spinlock, m_buffers,
                                   m_buffers.TakeOrMake(m_memory_resource),
                                   m_buffers.TakeOrMake(m_memory_resource)),
                including_self);

    return Status(std::move(builder));
  }

  Builder Initiate(std::string_view tx_description, Status *reuse = nullptr,
                   const DependentTransactions *dependent = nullptr) {
    DependentTransactions including_self{
        .prev = dependent,
        .length = nullptr == dependent ? 0 : 1 + dependent->length,
        .driver_name = m_name,
        .tx_description = tx_description};
    m_logger.Trace("{0:i}Initiating {0}", including_self);

    if (nullptr != reuse) {
      reuse->m_storage.ins.Clear();
      reuse->m_storage.outs.Clear();
      return Builder(std::move(reuse->m_storage), including_self);
    }

    SpinlockGuard lock(m_spinlock);
    return Builder(TransactionStorage(m_spinlock, m_buffers,
                                      m_buffers.TakeOrMake(m_memory_resource),
                                      m_buffers.TakeOrMake(m_memory_resource)),
                   including_self);
  }

  Builder Initiate(std::string_view tx_description, Status &&reuse) {
    return Initiate(tx_description, &reuse);
  }

  template <class TDependent>
  Builder Initiate(const TDependent &dependent,
                   std::string_view tx_description) {
    return Initiate(tx_description, nullptr, &dependent.GetDependent());
  }

  template <class TDependent>
  Builder Initiate(const TDependent &dependent, std::string_view tx_description,
                   Status &&reuse) {
    return Initiate(tx_description, &reuse, &dependent.GetDependent());
  }

  CheckedTask<Status> Schedule(Builder &&builder) {
    auto dependent = builder.GetDependent();
    m_logger.Trace("{0:i}[{0}] Scheduling", dependent);

    if (builder.HadAllocationFailure()) {
      m_logger.Error("{0:i}[{0}] Invalid transaction input: Allocation failure",
                     dependent);
      return CheckedTask<Status>::MakeResolved(std::move(builder));
    }

    Status before(std::move(builder));
    if (before) {
      m_logger.Trace("{:i1}Empty transaction, returning immediately",
                     dependent);
      return CheckedTask<Status>::MakeResolved(std::move(before));
    }

    TxInProgress tx(std::move(before));

    for (auto act : tx.Incomplete()) {
      if (!act) {
        std::optional<ActionID> action_id = act.GetID();

        if (action_id)
          m_logger.Error("{0:i}[{0}] Unknown or corrupted action with ID {1}",
                         dependent, *action_id);
        else
          m_logger.Error("{0:i}[{0}] Corrupted action", dependent);

        tx.template FailAt<CauseUnsupportedAction>(act);
        return CheckedTask<Status>::MakeResolved(tx.Finish());
      }

      act.Visit([this, &dependent](auto &action, auto &out) {
        m_logger.Trace("{:i1}{}", dependent, action);
      });
    }

    auto task = ScheduleAsync(dependent, std::move(tx));
    if (!task) {
      m_logger.Error("{0:i}[{0}] Failed to allocate a task frame, dropping",
                     dependent);
      return CheckedTask<Status>::MakeResolved(tx.Finish());
    }

    return task;
  }

  template <ActionType... Actions, class... Args>
  CheckedTask<Status> Do(std::string_view name, Args &&...args) {
    return DoImpl<Actions...>(name, nullptr, nullptr,
                              std::forward<Args>(args)...);
  }

  template <ActionType... Actions, class... Args>
  CheckedTask<Status> DoReuse(std::string_view name, Status &&reuse,
                              Args &&...args) {
    return DoImpl<Actions...>(name, &reuse, nullptr,
                              std::forward<Args>(args)...);
  }

  template <ActionType... Actions, class TDependent, class... Args>
  CheckedTask<Status> DoDep(const TDependent &dependent, std::string_view name,
                            Args &&...args) {
    return DoImpl<Actions...>(name, nullptr, &dependent.GetDependent(),
                              std::forward<Args>(args)...);
  }

  template <ActionType... Actions, class TDependent, class... Args>
  CheckedTask<Status> DoDepReuse(const TDependent &dependent,
                                 std::string_view name, Status &&reuse,
                                 Args &&...args) {
    return DoImpl<Actions...>(name, &reuse, &dependent.GetDependent(),
                              std::forward<Args>(args)...);
  }

  virtual bool Serve(bool wait_if_empty) = 0;

  virtual void Join(const std::coroutine_handle<> &to_complete) = 0;

  virtual void Terminate() = 0;

protected:
  std::pmr::memory_resource &m_memory_resource;
  Logger &m_logger;
  const std::string_view m_name;

  Spinlock m_spinlock;
  FreeList<TBuffer> m_buffers; // Under m_spinlock.

  virtual CheckedTask<Status> Execute(TxInProgress &&tx) = 0;

private:
  CheckedTask<Status> ScheduleAsync(DependentTransactions dependent,
                                    TxInProgress &&tx) {
    TxInProgress tx_captured(std::move(tx));

    auto task = Execute(std::move(tx_captured));
    if (!task) {
      m_logger.Error("{0:i}[{0}] Failed to allocate a task frame, dropping",
                     dependent);
      co_return tx_captured.Finish();
    }

    auto after = co_await task;
    if (m_logger.GetLevel() < LogLevel::TRACE)
      co_return after;

    m_logger.Trace("{0:i}[{0}] Completed", dependent);
    for (auto act : after.Complete())
      act.Visit([this, &dependent](auto &action, auto &out) {
        m_logger.Trace("{:i1}{} -> {}", dependent, action, out);
      });

    if (!after) {
      ActionError *error = after.GetError();
      if (nullptr == error)
        m_logger.Trace("{:i1}Unknown error", dependent);
      else
        m_logger.Trace("{:i1}{}", dependent, *error);
    }

    co_return after;
  }

  template <ActionType... Actions, class... Args>
  CheckedTask<Status> DoImpl(std::string_view name, Status *reuse,
                             const DependentTransactions *dependent,
                             Args &&...args) {
    auto xact = Initiate(name, reuse, dependent);

    if constexpr (0 == sizeof...(Actions))
      xact.template Add<Actions...>(std::forward<Args>(args)...);
    else
      (
          [&]() {
            ExpandArg<Args>::Do(
                [&]<class... EArgs>(EArgs &&...eargs) {
                  xact.template Add<Actions>(std::forward<EArgs>(eargs)...);
                },
                std::forward<Args>(args));
          }(),
          ...);

    return Schedule(std::move(xact));
  }
};

class Tunnel {
protected:
  template <class TAction>
  static TransactionStorage &
  GetTransactionStorage(TransactionBuilder<TAction> &builder) {
    return builder.m_storage;
  }

  template <class TAction>
  static TransactionStorage &
  GetTransactionStorage(TransactionInProgress<TAction> &tx) {
    return tx.m_status.m_storage;
  }

  template <class TAction>
  static TransactionStorage &
  GetTransactionStorage(TransactionStatus<TAction> &status) {
    return status.m_storage;
  }

  template <class TAction>
  static TransactionStatus<TAction>::ActionPosition &
  GetPositionOfFirstIncomplete(TransactionInProgress<TAction> &tx) {
    return tx.m_status.m_first_incomplete;
  }

  template <class TAction>
  static TransactionStatus<TAction>::ActionPosition &
  GetPositionOfFirstIncomplete(TransactionStatus<TAction> &status) {
    return status.m_first_incomplete;
  }

  template <class TAction>
  static std::optional<TBuffer> &
  GetErrorBuffer(TransactionInProgress<TAction> &tx) {
    return tx.m_status.m_error_buffer;
  }

  template <class TAction>
  static std::optional<TBuffer> &
  GetErrorBuffer(TransactionStatus<TAction> &status) {
    return status.m_error_buffer;
  }
};

} // namespace edr

template <> struct std::formatter<edr::DriverID, char> {
  template <class ParseContext>
  constexpr ParseContext::iterator parse(ParseContext &ctx) {
    auto it = ctx.begin();
    while (*it != '}')
      it++;

    return it;
  }

  template <class FmtContext>
  FmtContext::iterator format(const edr::DriverID &id, FmtContext &ctx) const {
    return std::format_to(ctx.out(), "{}", static_cast<uint32_t>(id));
  }
};

#endif
