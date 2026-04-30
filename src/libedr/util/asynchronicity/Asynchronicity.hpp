#ifndef LIBEDR_UTIL_ASYNCHRONICITY_ASYNCHRONICITY_HPP
#define LIBEDR_UTIL_ASYNCHRONICITY_ASYNCHRONICITY_HPP

#include <atomic>
#include <cassert>
#include <concepts>
#include <coroutine>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace edr {

inline constexpr std::coroutine_handle<> g_task_state_initial = nullptr;
inline const std::coroutine_handle<> g_task_state_result_ready =
    std::coroutine_handle<>::from_address(reinterpret_cast<void *>(1ULL));
inline const std::coroutine_handle<> g_task_state_awaitable_destroyed =
    std::coroutine_handle<>::from_address(reinterpret_cast<void *>(2ULL));

template <class... R> struct TaskResultHelper {
  using Type = std::tuple<R...>;
  using Ref = Type &;
};

template <> struct TaskResultHelper<> {
  using Type = void;
  using Ref = void;
};

template <class R> struct TaskResultHelper<R> {
  using Type = R;
  using Ref = Type &;
};

namespace {

template <class T>
concept TaskOwner = requires(T asynchronous, void *ptr, size_t size) {
  {
    asynchronous.GetFrameAllocator().allocate(size)
  } -> std::convertible_to<void *>;
  {
    asynchronous.GetFrameAllocator().deallocate(
        static_cast<T::FrameAllocator::value_type *>(ptr), size)
  };
};

} // namespace

template <bool t_eager_continuation, class... R> struct Resolver;

template <class... R> using EagerResolver = Resolver<true, R...>;
template <class... R> using DelayedResolver = Resolver<false, R...>;

template <class P, class... R> struct AwaitableBase {
public:
  using ResultType = TaskResultHelper<R...>::Type;

  bool await_ready() const noexcept {
    if (!IsValid())
      return true;

    return static_cast<const P *>(this)->ForResolver([&](auto &resolver) {
      return g_task_state_initial != resolver.m_continuation.load();
    });
  }

  bool await_suspend(std::coroutine_handle<> continuation) noexcept {
    assert(IsValid());

    return static_cast<P *>(this)->ForResolver([&](auto &resolver) {
      auto expected = g_task_state_initial;
      auto awaited = resolver.m_continuation.compare_exchange_strong(
          expected, continuation);
      return awaited;
    });
  }

  ResultType await_resume() {
    if constexpr (0 == sizeof...(R))
      return;
    else
      return TakeResult();
  }

  ResultType TakeResult() {
    if constexpr (0 == sizeof...(R))
      return;
    else if (!IsValid())
      return static_cast<P *>(this)->TakeFallbackResult();
    else
      return static_cast<P *>(this)->ForResolver(
          [&](auto &resolver) -> ResultType && {
            return std::move(*resolver.m_result);
          });
  }

  ResultType *operator->() {
    assert(IsValid());

    if constexpr (0 == sizeof...(R))
      return nullptr;
    else
      return static_cast<P *>(this)->ForResolver(
          [&](auto &resolver) -> ResultType * { return &*resolver.m_result; });
  }

  TaskResultHelper<R...>::Ref operator*() {
    assert(IsValid());

    if constexpr (0 == sizeof...(R))
      return;
    else
      return *operator->();
  }

private:
  bool IsValid() const { return static_cast<const P *>(this)->operator bool(); }
};

struct ContinuationStorage {
  std::coroutine_handle<> HandleResultReady() {
    auto previous_state = m_continuation.exchange(g_task_state_result_ready);
    if (g_task_state_initial == previous_state)
      return previous_state;

    if (g_task_state_awaitable_destroyed == previous_state) {
      m_continuation.store(previous_state);
      return previous_state;
    }

    previous_state.resume();
    return previous_state;
  }

  std::atomic<std::coroutine_handle<>> m_continuation = g_task_state_initial;
};

template <bool t_default_init_if_needed, class... R> struct FallbackStorage {
  using StorageType = TaskResultHelper<R...>::Type;

  FallbackStorage() = default;

  FallbackStorage(FallbackStorage &&from) : m_result(std::move(from.m_result)) {
    from.m_result.reset();
  }

  FallbackStorage(const FallbackStorage &) = delete;
  FallbackStorage &operator=(const FallbackStorage &) = delete;
  FallbackStorage &operator=(FallbackStorage &&) = delete;

  template <class... Args> void Emplace(Args &&...args) {
    m_result.emplace(std::forward<Args>(args)...);
  }

  StorageType &GetResult() {
    if constexpr (t_default_init_if_needed)
      if (!m_result)
        m_result.emplace();

    return *m_result;
  }

private:
  std::optional<StorageType> m_result;
};

template <bool t_default_init_if_needed>
struct FallbackStorage<t_default_init_if_needed> {
  void Emplace() {}

  void GetResult() {}
};

template <class... R>
struct AwaitableRef : public AwaitableBase<AwaitableRef<R...>, R...> {
  AwaitableRef(EagerResolver<R...> &resolver) : m_resolver(resolver) {}

  template <class F> auto ForResolver(F &&func) {
    return std::forward<F>(func)(m_resolver);
  }

  template <class F> auto ForResolver(F &&func) const {
    return std::forward<F>(func)(m_resolver);
  }

  TaskResultHelper<R...>::Type TakeFallbackResult() {
    return std::move(m_fallback.GetResult());
  }

  operator bool() const { return true; }

  EagerResolver<R...> &m_resolver;
  FallbackStorage<true, R...> m_fallback;
};

template <bool t_eager_continuation, class... R>
struct Resolver : public ContinuationStorage {
  Resolver() = default;

  using StorageType = TaskResultHelper<R...>::Type;

  template <class... Args> void return_value(Args &&...result) {
    m_result.emplace(std::forward<Args>(result)...);
    if constexpr (t_eager_continuation)
      this->HandleResultReady();
  }

  std::optional<StorageType> m_result;
};

template <bool t_eager_continuation>
struct Resolver<t_eager_continuation> : public ContinuationStorage {
  void return_void() {
    if constexpr (t_eager_continuation)
      this->HandleResultReady();
  }
};

template <bool t_def_init, TaskOwner Owner, class... R> struct Promise;

template <bool t_def_init, TaskOwner Owner, class... R>
class OwnedTask final
    : public std::coroutine_handle<Promise<t_def_init, Owner, R...>>,
      public AwaitableBase<OwnedTask<t_def_init, Owner, R...>, R...> {
  using Base = std::coroutine_handle<Promise<t_def_init, Owner, R...>>;

public:
  using promise_type = Promise<t_def_init, Owner, R...>;

  template <class... Args> static OwnedTask MakeResolved(Args &&...args) {
    OwnedTask task(nullptr);
    task.m_fallback.Emplace(std::forward<Args>(args)...);
    return task;
  }

  OwnedTask(OwnedTask &&from)
      : Base(from), m_fallback(std::move(from.m_fallback)) {
    static_cast<Base &>(from) = nullptr;
  }

  OwnedTask(const OwnedTask &) = delete;
  OwnedTask &operator=(const OwnedTask &) = delete;
  OwnedTask &operator=(OwnedTask &&) = delete;

  explicit OwnedTask(std::nullptr_t) : Base(nullptr) {}

  explicit OwnedTask(promise_type &promise)
      : Base(Base::from_promise(promise)) {}

  ~OwnedTask() {
    if (!(*this))
      return;

    auto previous_state = this->promise().m_continuation.exchange(
        g_task_state_awaitable_destroyed);
    if (g_task_state_initial == previous_state ||
        g_task_state_result_ready == previous_state)
      return;

    assert(g_task_state_awaitable_destroyed != previous_state);

    previous_state.destroy();
  }

  void destroy() const = delete;

  void destroy() {
    if (!(*this))
      return;

    Base::destroy();
    *static_cast<Base *>(this) = nullptr;
  }

  bool done() {
    if (!(*this))
      return true;

    using Base = std::coroutine_handle<promise_type>;
    return Base::done();
  }

  template <class F> auto ForResolver(F &&func) {
    assert(*this);
    return std::forward<F>(func)(this->promise());
  }

  template <class F> auto ForResolver(F &&func) const {
    assert(*this);
    return std::forward<F>(func)(this->promise());
  }

  TaskResultHelper<R...>::Type TakeFallbackResult() {
    return std::move(m_fallback.GetResult());
  }

private:
  FallbackStorage<t_def_init, R...> m_fallback;
};

template <TaskOwner Owner> struct OwnedFrame {
  template <class... Args>
  static void *operator new(std::size_t size, Owner &self,
                            Args &&...args) noexcept {
    size_t this_ptr_size = sizeof(Owner *);
    std::byte *ptr = reinterpret_cast<std::byte *>(
        self.GetFrameAllocator().allocate(this_ptr_size + size));
    if (nullptr == ptr)
      return nullptr;

    *reinterpret_cast<Owner **>(ptr) = &self;
    return ptr + this_ptr_size;
  }

  static void operator delete(void *ptr, std::size_t size) noexcept {
    size_t this_ptr_size = sizeof(Owner *);

    std::byte *byte_ptr = reinterpret_cast<std::byte *>(ptr);
    std::byte *full_ptr = byte_ptr - this_ptr_size;

    Owner *self = *reinterpret_cast<Owner **>(full_ptr);
    self->GetFrameAllocator().deallocate(full_ptr, this_ptr_size + size);
  }
};

template <bool t_def_init, TaskOwner Owner, class... R>
struct Promise : public DelayedResolver<R...>, public OwnedFrame<Owner> {
  static OwnedTask<t_def_init, Owner, R...>
  get_return_object_on_allocation_failure() {
    return OwnedTask<t_def_init, Owner, R...>(nullptr);
  }

  std::suspend_never initial_suspend() noexcept { return {}; }

  auto final_suspend() noexcept {
    struct FinalDecider {
      FinalDecider(Promise<t_def_init, Owner, R...> &self) : m_self(self) {}

      bool await_ready() const noexcept {
        return g_task_state_awaitable_destroyed == m_self.m_continuation.load();
      }

      bool await_suspend(std::coroutine_handle<> continuation) const noexcept {
        auto expected = g_task_state_result_ready;
        auto awaited = m_self.m_continuation.compare_exchange_strong(
            expected, continuation);
        return awaited;
      }

      void await_resume() const noexcept {}

      Promise<t_def_init, Owner, R...> &m_self;
    };

    this->HandleResultReady();
    return FinalDecider(*this);
  }

  void unhandled_exception() {}

  OwnedTask<t_def_init, Owner, R...> get_return_object() {
    return OwnedTask<t_def_init, Owner, R...>(*this);
  }
};

template <class... Args> struct GeneratorArguments {
  using Type = TaskResultHelper<Args...>::Type;

  template <class... TArgs> void Set(TArgs &&...new_args) {
    if constexpr (1 == sizeof...(new_args))
      args.emplace(std::forward<TArgs>(new_args)...);
    else
      args.emplace(Type{std::forward<TArgs>(new_args)...});
  }

  bool await_ready() const noexcept { return args.has_value(); }

  void await_suspend(std::coroutine_handle<> continuation) noexcept {}

  Type await_resume() {
    Type result = std::move(*args);
    args.reset();
    return result;
  }

  std::optional<Type> args;
};

template <> struct GeneratorArguments<> {
  using Type = void;

  template <class... TArgs> void Set(TArgs &&...new_args) { ready = true; }

  bool await_ready() const noexcept { return ready; }

  void await_suspend(std::coroutine_handle<> continuation) noexcept {}

  void await_resume() { ready = false; }

  bool ready = false;
};

template <TaskOwner Owner, class TPromise> struct OwnedGenerator;

template <TaskOwner Owner, class TGeneratorArguments, class... R>
struct GeneratorPromise final : public OwnedFrame<Owner> {
  using ResultType = TaskResultHelper<R...>::Type;

  OwnedGenerator<Owner, GeneratorPromise> get_return_object() {
    return OwnedGenerator<Owner, GeneratorPromise>(this);
  }

  static OwnedGenerator<Owner, GeneratorPromise>
  get_return_object_on_allocation_failure() {
    return OwnedGenerator<Owner, GeneratorPromise>(nullptr);
  }

  std::suspend_never initial_suspend() noexcept { return {}; }
  std::suspend_always final_suspend() noexcept { return {}; }

  template <class Arg> TGeneratorArguments &yield_value(Arg &&arg) {
    result.emplace(std::forward<Arg>(arg));
    return args;
  }

  void return_void() {}

  void unhandled_exception() {}

  TGeneratorArguments args;
  std::optional<ResultType> result;
};

template <TaskOwner Owner, class TPromise> struct OwnedGenerator {
  using promise_type = TPromise;

  OwnedGenerator(TPromise *promise)
      : m_handle(
            nullptr == promise
                ? std::coroutine_handle<TPromise>(nullptr)
                : std::coroutine_handle<TPromise>::from_promise(*promise)) {}

  OwnedGenerator(OwnedGenerator &&from) : m_handle(from) {
    from.m_handle = nullptr;
  }

  OwnedGenerator(const OwnedGenerator &) = delete;
  OwnedGenerator &operator=(const OwnedGenerator &) = delete;
  OwnedGenerator &operator=(OwnedGenerator &&) = delete;

  ~OwnedGenerator() {
    if (m_handle)
      m_handle.destroy();
  }

  operator bool() { return m_handle && !m_handle.done(); }

  template <class... TArgs> TPromise::ResultType operator()(TArgs &&...args) {
    assert(m_handle);

    auto &promise = m_handle.promise();
    promise.args.Set(std::forward<TArgs>(args)...);

    m_handle.resume();

    return std::move(*promise.result);
  }

private:
  std::coroutine_handle<TPromise> m_handle;
};

template <class A = std::allocator<std::byte>> class Asynchronous {
public:
  using FrameAllocator = A;
  static_assert(sizeof(typename A::value_type) == sizeof(std::byte));

  template <class... R> using Task = OwnedTask<true, Asynchronous<A>, R...>;
  template <class... R>
  using CheckedTask = OwnedTask<false, Asynchronous<A>, R...>;

  template <class TGeneratorArguments, class... R>
  using Generator = OwnedGenerator<
      Asynchronous<A>,
      GeneratorPromise<Asynchronous<A>, TGeneratorArguments, R...>>;

  Asynchronous(const A &allocator = A{}) : m_allocator(allocator) {}

  A &GetFrameAllocator() { return m_allocator; }

private:
  A m_allocator;

  Asynchronous(const Asynchronous &) = delete;
  Asynchronous(Asynchronous &&) = delete;

  Asynchronous &operator=(const Asynchronous &) = delete;
  Asynchronous &operator=(Asynchronous &&) = delete;
};

} // namespace edr

#endif
