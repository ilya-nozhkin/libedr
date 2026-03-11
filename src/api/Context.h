#ifndef LIBEDR_API_LOGGER_H
#define LIBEDR_API_LOGGER_H

#include "Error.h"

#include "libedr/driver/Driver.hpp"
#include "libedr/driver/Logger.hpp"
#include "libedr/util/asynchronicity/Asynchronicity.hpp"
#include "libedr/util/memory/FreeListAllocator.hpp"

#include <format>
#include <mutex>
#include <type_traits>
#include <utility>

enum class LogLevel { ERROR = 0, WARNING = 1, INFO = 2, DEBUG = 3, TRACE = 4 };

#ifndef SWIG
inline edr::LogLevel ToEDRLogLevel(LogLevel log_level) {
  switch (log_level) {
  case LogLevel::ERROR:
    return edr::LogLevel::ERROR;
  case LogLevel::WARNING:
    return edr::LogLevel::WARNING;
  case LogLevel::INFO:
    return edr::LogLevel::INFO;
  case LogLevel::DEBUG:
    return edr::LogLevel::DEBUG;
  case LogLevel::TRACE:
    return edr::LogLevel::TRACE;

  default:
    return edr::LogLevel::TRACE;
  }
}

class TransactionNameGuard : public edr::FreeListAsynchronous {
public:
  using Cookie = size_t;

  TransactionNameGuard(edr::DriverContext &ctx)
      : edr::FreeListAsynchronous(ctx.TaskFrameResource()) {}

  std::pair<Cookie, std::string_view> AllocateName(const char *name) {
    std::scoped_lock<std::mutex> lock(m_mutex);

    if (!m_free_slots.empty()) {
      size_t slot = m_free_slots.back();
      m_free_slots.pop_back();

      auto &buffer = m_slots[slot];
      buffer = name;

      return std::make_pair(slot, std::string_view(buffer));
    }

    size_t slot = m_slots.size();
    auto &emplaced = m_slots.emplace_back(name);
    return std::make_pair(slot, std::string_view(emplaced));
  }

  template <class T, class B>
  Task<typename T::Status> Schedule(Cookie slot, T &driver, B &&builder) {
    auto status = co_await driver.Schedule(std::forward<B>(builder));

    std::scoped_lock<std::mutex> lock(m_mutex);
    m_free_slots.push_back(slot);

    co_return status;
  }

private:
  std::mutex m_mutex;
  std::vector<std::string> m_slots;
  std::vector<size_t> m_free_slots;
};

#endif

class Context {
public:
  Context(LogLevel log_level)
      : m_logger(m_output, ToEDRLogLevel(log_level)),
        m_context({.logger = m_logger}), m_transaction_name_guard(m_context) {}

  Context(const Context &) = delete;
  Context(Context &&) = delete;
  Context &operator=(const Context &) = delete;
  Context &operator=(Context &&) = delete;

  ~Context() {
    // Deleting entities in reverse order so that dependent ones can access
    // their dependencies from destructors.
    for (auto it = m_entities.rbegin(); it != m_entities.rend(); it++)
      it->reset();
  }

  void AddStdStreams() { m_output.AddStdStreams(); }

  void AddFile(const char *filename) { m_output.AddFile(filename); }

  void SetLogLevel(LogLevel log_level) {
    m_logger.SetLevel(ToEDRLogLevel(log_level));
  }

#ifndef SWIG
  const edr::DriverContext &Get() { return m_context; }

  template <class T, class... Args> T &Make(Args &&...args) {
    auto entity_sp = std::make_shared<T>(std::forward<Args>(args)...);

    std::scoped_lock lock(m_entities_mutex);
    auto &emplaced = m_entities.emplace_back(std::move(entity_sp));
    return *reinterpret_cast<T *>(emplaced.get());
  }

  template <class T, class... Args> T &MakeWith(Args &&...args) {
    return Make<T>(m_context, std::forward<Args>(args)...);
  }

  template <class T> T &Store(T &&entity) { return Make<T>(std::move(entity)); }

  template <class F, class... Args>
  auto *CallWith(Error &error, F &&func, Args &&...args) {
    auto exp_result = func(m_context, std::forward<Args>(args)...);
    if (exp_result)
      return &Store(std::move(*exp_result));

    error = std::move(exp_result.error());
    return static_cast<std::remove_reference_t<decltype(*exp_result)> *>(
        nullptr);
  }

  template <class... Args>
  const char *PersistFormat(std::format_string<Args...> format,
                            Args &&...args) {
    std::string formatted = std::format(format, std::forward<Args>(args)...);
    auto &emplaced = Store(std::move(formatted));
    return emplaced.c_str();
  }

  TransactionNameGuard &GetNameGuard() { return m_transaction_name_guard; }
#endif

private:
  class LoggerOutput final : public edr::LoggerOutput {
  public:
    LoggerOutput() = default;

    void AddStdStreams() { m_std_output.emplace(); }

    void AddFile(const char *filename) { m_file_output.emplace(filename); }

    void AddLine(edr::LogLevel level, std::string_view message) override {
      if (m_std_output)
        m_std_output->AddLine(level, message);

      if (m_file_output)
        m_file_output->AddLine(level, message);
    }

  private:
    std::optional<edr::StdStreamsLoggerOutput> m_std_output;
    std::optional<edr::FileLoggerOutput> m_file_output;
  };

  LoggerOutput m_output;
  edr::Logger m_logger;
  edr::DriverContext m_context;

  std::mutex m_entities_mutex;
  std::vector<std::shared_ptr<void>> m_entities;

  TransactionNameGuard m_transaction_name_guard;
};

#endif
