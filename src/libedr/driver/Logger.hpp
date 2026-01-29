#ifndef LIBEDR_DRIVER_LOGGER_HPP
#define LIBEDR_DRIVER_LOGGER_HPP

#include "libedr/util/miscellaneous/Spinlock.hpp"

#include <format>
#include <memory_resource>
#include <string_view>
#include <utility>

namespace edr {

enum class LogLevel { ERROR = 0, WARNING = 1, INFO = 2, DEBUG = 3, TRACE = 4 };

class LoggerOutput {
public:
  virtual void AddLine(LogLevel level, std::string_view message) = 0;
};

class StdStreamsLoggerOutput : public LoggerOutput {
public:
  void AddLine(LogLevel level, std::string_view message) {
    if (LogLevel::ERROR == level || LogLevel::WARNING == level) {
      fwrite(message.data(), 1, message.size(), stderr);
      fwrite("\n", 1, 1, stderr);
    } else {
      fwrite(message.data(), 1, message.size(), stdout);
      fwrite("\n", 1, 1, stdout);
    }
  }
};

class FileLoggerOutput : public LoggerOutput {
public:
  FileLoggerOutput(const char *filename) { m_file = fopen(filename, "w"); }

  FileLoggerOutput(FileLoggerOutput &&from) : m_file(from.m_file) {
    from.m_file = nullptr;
  }

  FileLoggerOutput(const FileLoggerOutput &) = delete;
  FileLoggerOutput &operator=(const FileLoggerOutput &) = delete;
  FileLoggerOutput &operator=(FileLoggerOutput &&) = delete;

  ~FileLoggerOutput() {
    if (nullptr != m_file)
      fclose(m_file);
  }

  void AddLine(LogLevel level, std::string_view message) {
    if (nullptr != m_file) {
      fwrite(message.data(), 1, message.size(), m_file);
      fwrite("\n", 1, 1, m_file);
    }
  }

private:
  FILE *m_file;
};

class Logger final {
public:
  Logger(LoggerOutput &out, LogLevel level,
         std::pmr::memory_resource *memory_resource = nullptr)
      : m_out(out), m_log_level(level),
        m_resource(nullptr != memory_resource
                       ? *memory_resource
                       : *std::pmr::get_default_resource()) {}

  void SetLevel(LogLevel level) { m_log_level = level; }

  LogLevel GetLevel() { return m_log_level; }

  template <class... Args>
  void Log(LogLevel level, std::format_string<Args...> format, Args &&...args) {
    size_t prefix_size = std::formatted_size("{}", GetPrefix(level));
    size_t size = std::formatted_size(format, std::forward<Args>(args)...);

    SpinlockGuard lock(m_spinlock);
    size_t total = prefix_size + size;
    if (!Allocate(total))
      return;

    std::format_to_n(m_buffer, prefix_size, "{}", GetPrefix(level));
    std::format_to_n(m_buffer + prefix_size, size, format,
                     std::forward<Args>(args)...);
    m_out.AddLine(level, std::string_view(m_buffer, total));
  }

  template <class... Args>
  void Error(std::format_string<Args...> format, Args &&...args) {
    if (m_log_level >= LogLevel::ERROR)
      Log(LogLevel::ERROR, format, std::forward<Args>(args)...);
  }

  template <class... Args>
  void Warning(std::format_string<Args...> format, Args &&...args) {
    if (m_log_level >= LogLevel::WARNING)
      Log(LogLevel::WARNING, format, std::forward<Args>(args)...);
  }

  template <class... Args>
  void Info(std::format_string<Args...> format, Args &&...args) {
    if (m_log_level >= LogLevel::INFO)
      Log(LogLevel::INFO, format, std::forward<Args>(args)...);
  }

  template <class... Args>
  void Debug(std::format_string<Args...> format, Args &&...args) {
    if (m_log_level >= LogLevel::DEBUG)
      Log(LogLevel::DEBUG, format, std::forward<Args>(args)...);
  }

  template <class... Args>
  void Trace(std::format_string<Args...> format, Args &&...args) {
    if (m_log_level >= LogLevel::TRACE)
      Log(LogLevel::TRACE, format, std::forward<Args>(args)...);
  }

private:
  const char *GetPrefix(LogLevel level) {
    switch (level) {
    case LogLevel::ERROR:
      return "[ERROR] ";
    case LogLevel::INFO:
      return "[INFO] ";
    case LogLevel::DEBUG:
      return "[DEBUG] ";
    case LogLevel::TRACE:
      return "[TRACE] ";

    default:
      return "[WARNING] ";
    }
  }

  bool Allocate(size_t size) {
    if (size <= m_size)
      return true;

    m_resource.deallocate(m_buffer, m_size);

    m_buffer = reinterpret_cast<char *>(m_resource.allocate(size));
    if (nullptr == m_buffer) {
      m_size = 0;
      m_out.AddLine(LogLevel::ERROR, "Failed to allocate a log line buffer!");
      return false;
    }

    m_size = size;
    return true;
  }

  LoggerOutput &m_out;
  LogLevel m_log_level;
  std::pmr::memory_resource &m_resource;

  Spinlock m_spinlock;
  char *m_buffer = nullptr;
  size_t m_size = 0;
};

} // namespace edr

#endif
