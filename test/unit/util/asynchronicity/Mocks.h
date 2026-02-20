#ifndef COMMON_ASYNCHRONICITY_MOCKS_H
#define COMMON_ASYNCHRONICITY_MOCKS_H

#include "libedr/util/asynchronicity/Asynchronicity.hpp"

#include <gmock/gmock.h>

#include <memory>
#include <string>
#include <string_view>

namespace edr {

class ResultListener {
public:
  MOCK_METHOD(void, Constructor, ());
  MOCK_METHOD(void, FinalDestructor, ());
  MOCK_METHOD(void, Copy, ());
  MOCK_METHOD(void, Move, ());
};

class Result {
public:
  Result() : m_listener(nullptr) {}

  Result(ResultListener &listener, std::string_view payload)
      : m_listener(&listener), m_payload(payload) {
    assert(m_listener);
    m_listener->Constructor();
  }

  Result(const Result &source)
      : m_listener(source.m_listener), m_payload(source.m_payload) {
    assert(m_listener);
    m_listener->Copy();
  }

  Result(Result &&source)
      : m_listener(source.m_listener), m_payload(std::move(source.m_payload)) {
    assert(m_listener);
    source.m_listener = nullptr;
    m_listener->Move();
  }

  ~Result() {
    if (nullptr != m_listener)
      m_listener->FinalDestructor();
  }

  std::string_view GetPayload() { return m_payload; }

private:
  ResultListener *m_listener;
  std::string m_payload;
};

class Endpoint {
public:
  AwaitableRef<> InitiateEmpty() {
    m_empty_resolver.emplace();
    return *m_empty_resolver;
  }

  void ResolveEmpty() {
    assert(m_empty_resolver.has_value());
    m_empty_resolver->return_void();
  }

  AwaitableRef<Result> Initiate() {
    m_result_resolver.emplace();
    return *m_result_resolver;
  }

  void Resolve(ResultListener &listener, const std::string_view &payload) {
    assert(m_result_resolver.has_value());
    m_result_resolver->return_value(Result(listener, payload));
  }

  bool EmptyResolverIsPending() { return m_empty_resolver.has_value(); }

  bool ResultResolverIsPending() { return m_result_resolver.has_value(); }

private:
  std::optional<Resolver<>> m_empty_resolver;
  std::optional<Resolver<Result>> m_result_resolver;
};

class MockAllocator {
public:
  MOCK_METHOD(void *, allocate, (std::size_t size), ());
  MOCK_METHOD(void, deallocate, (void *ptr, std::size_t size), ());
};

class MockAllocatorProxy {
public:
  using value_type = std::byte;

  MockAllocatorProxy(MockAllocator &allocator) : m_allocator(allocator) {}

  void *allocate(std::size_t size) { return m_allocator.allocate(size); }

  void deallocate(void *ptr, std::size_t size) {
    m_allocator.deallocate(ptr, size);
  }

private:
  MockAllocator &m_allocator;
};

template <class A = std::allocator<std::byte>>
class MockAsynchronous : public Asynchronous<A> {
public:
  template <class... R> using Task = Asynchronous<A>::template Task<R...>;

  MockAsynchronous(Endpoint &endpoint, const A &allocator = {})
      : Asynchronous<A>(allocator), m_endpoint(endpoint) {}

  Task<> TrivialEndpoint() { co_return; }

  Task<> TrivialDependent() {
    co_await TrivialEndpoint();
    TrivialDependentFinished();
  }

  Task<> Resolvable() {
    co_await m_endpoint.InitiateEmpty();
    ResolvableFinished();
  }

  Task<> ResolvableDependent() {
    co_await Resolvable();
    ResolvableDependentFinished();
  }

  Task<Result> ResolvableWithResult() {
    auto result = co_await m_endpoint.Initiate();
    co_return result;
  }

  Task<Result> ResolvingBeforeAwait(ResultListener &listener,
                                    const std::string_view &payload) {
    auto task = m_endpoint.Initiate();
    m_endpoint.Resolve(listener, payload);

    auto result = co_await task;
    co_return result;
  }

  MOCK_METHOD(void, TrivialDependentFinished, ());
  MOCK_METHOD(void, ResolvableFinished, ());
  MOCK_METHOD(void, ResolvableDependentFinished, ());

private:
  Endpoint &m_endpoint;
};

} // namespace edr

#endif
