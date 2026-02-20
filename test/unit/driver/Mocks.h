#ifndef DRIVER_MOCKS_H
#define DRIVER_MOCKS_H

#include "libedr/driver/Logger.hpp"

#include "gmock/gmock.h"

#include <cstddef>
#include <memory_resource>

namespace edr {

class DisableableMemoryResource : public std::pmr::memory_resource {
public:
  void Enable() { m_enabled = true; }

  void Disable() { m_enabled = false; }

private:
  void *do_allocate(size_t bytes, size_t alignment) override {
    if (!m_enabled)
      return nullptr;

    return std::pmr::get_default_resource()->allocate(bytes, alignment);
  }

  void do_deallocate(void *ptr, size_t bytes, size_t alignment) override {
    std::pmr::get_default_resource()->deallocate(ptr, bytes, alignment);
  }

  bool do_is_equal(const memory_resource &other) const noexcept override {
    return std::pmr::get_default_resource()->is_equal(other);
  }

  bool m_enabled = true;
};

class MockLoggerOutput final : public LoggerOutput {
public:
  MockLoggerOutput(LogLevel check_down_to) : m_check_down_to(check_down_to) {}

  void AddLine(LogLevel level, std::string_view message) {
    if (LogLevel::ERROR == level || LogLevel::WARNING == level) {
      fwrite(message.data(), 1, message.size(), stderr);
      fwrite("\n", 1, 1, stderr);
    } else {
      fwrite(message.data(), 1, message.size(), stdout);
      fwrite("\n", 1, 1, stdout);
    }

    if (static_cast<size_t>(level) <= static_cast<size_t>(m_check_down_to))
      CheckLine(level, message);
  }

  MOCK_METHOD(void, CheckLine, (LogLevel level, std::string_view message), ());

private:
  LogLevel m_check_down_to;
};

} // namespace edr

#endif
