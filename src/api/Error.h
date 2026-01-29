#ifndef LIBEDR_API_ERROR_H
#define LIBEDR_API_ERROR_H

#include "libedr/driver/Error.hpp"

#include <format>

class Error {
public:
  Error() : m_error(edr::Error::Success()) {}

  Error(const Error &) = delete;
  Error &operator=(const Error &) = delete;

  Error(Error &&) = default;

#ifndef SWIG
  Error &operator=(Error &&) = default;

  Error(edr::Error &&error) : m_error(std::move(error)) {}

  Error &operator=(edr::Error &&error) {
    m_error = std::move(error);
    m_string.clear();
    return *this;
  }
#endif

  const char *Message() {
    if (!m_string.empty())
      return m_string.c_str();

    size_t size = std::formatted_size("{}", m_error);
    m_string.resize_and_overwrite(size, [this](char *dest, size_t dest_size) {
      return std::format_to_n(dest, dest_size, "{}", m_error).size;
    });

    return m_string.c_str();
  }

private:
  edr::Error m_error;
  std::string m_string;
};

#endif
