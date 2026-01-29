#ifndef LIBEDR_API_COMMON_H
#define LIBEDR_API_COMMON_H

#include <memory>

#define TRANSACTION_BODY(DRIVER_API_NAME, DRIVER_LIB_NAME)                     \
  friend class DRIVER_API_NAME;                                                \
                                                                               \
public:                                                                        \
  DRIVER_API_NAME##Transaction(const DRIVER_API_NAME##Transaction &) = delete; \
  DRIVER_API_NAME##Transaction &operator=(                                     \
      const DRIVER_API_NAME##Transaction &) = delete;                          \
  DRIVER_API_NAME##Transaction(DRIVER_API_NAME##Transaction &&) = default;     \
  DRIVER_API_NAME##Transaction &operator=(DRIVER_API_NAME##Transaction &&) =   \
      delete;                                                                  \
                                                                               \
  bool Schedule() {                                                            \
    if (!m_builder)                                                            \
      return false;                                                            \
                                                                               \
    m_task.emplace(m_driver->Schedule(std::move(*m_builder)));                 \
    m_builder.reset();                                                         \
    return true;                                                               \
  }                                                                            \
                                                                               \
  bool Done() { return !m_task || !(*m_task) || m_task->done(); }              \
                                                                               \
  bool Success() { return m_task && *m_task && **m_task; }                     \
                                                                               \
  bool Fail() { return !Success(); }                                           \
                                                                               \
  const char *ErrorMessage() {                                                 \
    if (!m_task)                                                               \
      return "Not scheduled";                                                  \
                                                                               \
    if (!(*m_task))                                                            \
      return "Failed to schedule due to allocation failure";                   \
                                                                               \
    auto *error = m_task.value()->GetError();                                  \
    if (nullptr == error) {                                                    \
      if (Success())                                                           \
        return "";                                                             \
                                                                               \
      return "Unknown or corrupted error";                                     \
    }                                                                          \
                                                                               \
    size_t size = std::formatted_size("{}", *error);                           \
    m_error_message.resize_and_overwrite(                                      \
        size, [&error](char *dest, size_t dest_size) {                         \
          return std::format_to_n(dest, dest_size, "{}", *error).size;         \
        });                                                                    \
                                                                               \
    return m_error_message.c_str();                                            \
  }                                                                            \
                                                                               \
  void Join() {                                                                \
    if (!m_task || !(*m_task))                                                 \
      return;                                                                  \
                                                                               \
    m_driver->Join(*m_task);                                                   \
  }                                                                            \
                                                                               \
  void Next() {                                                                \
    if (!InitCheckIterator())                                                  \
      return;                                                                  \
                                                                               \
    ++(*m_iterator);                                                           \
  }                                                                            \
                                                                               \
private:                                                                       \
  DRIVER_API_NAME##Transaction(                                                \
      const std::shared_ptr<Context> &context_sp, DRIVER_LIB_NAME *driver,     \
      std::unique_ptr<char[]> &&name, DRIVER_LIB_NAME::Builder &&builder)      \
      : m_context_sp(context_sp), m_driver(driver), m_name(std::move(name)),   \
        m_builder(std::move(builder)) {}                                       \
                                                                               \
  bool InitCheckIterator() {                                                   \
    if (!m_task || !(*m_task) || !m_task->done())                              \
      return false;                                                            \
                                                                               \
    if (!m_iterator.has_value())                                               \
      m_iterator.emplace((*m_task)->begin());                                  \
                                                                               \
    return *m_iterator != (*m_task)->end();                                    \
  }                                                                            \
                                                                               \
  DRIVER_API_NAME##Transaction() = default;                                    \
                                                                               \
  std::shared_ptr<Context> m_context_sp;                                       \
  DRIVER_LIB_NAME *m_driver = nullptr;                                         \
  std::unique_ptr<char[]> m_name;                                              \
  std::string m_error_message;                                                 \
  std::optional<DRIVER_LIB_NAME::Builder> m_builder;                           \
  std::optional<DRIVER_LIB_NAME::CheckedTask<DRIVER_LIB_NAME::Status>> m_task; \
  std::optional<DRIVER_LIB_NAME::Status::Iterator> m_iterator;

#ifdef SWIG
#define EXTERNAL_DRIVER_CONSTRUCTOR(DRIVER_API_NAME, DRIVER_LIB_NAME)
#else
#define EXTERNAL_DRIVER_CONSTRUCTOR(DRIVER_API_NAME, DRIVER_LIB_NAME)          \
  DRIVER_API_NAME(const std::shared_ptr<Context> &context_sp,                  \
                  DRIVER_LIB_NAME *driver = nullptr)                           \
      : m_context_sp(context_sp), m_driver(driver) {}
#endif

#define DRIVER_BODY(DRIVER_API_NAME, DRIVER_LIB_NAME)                          \
public:                                                                        \
  EXTERNAL_DRIVER_CONSTRUCTOR(DRIVER_API_NAME, DRIVER_LIB_NAME)                \
                                                                               \
  DRIVER_API_NAME(const DRIVER_API_NAME &) = delete;                           \
  DRIVER_API_NAME &operator=(const DRIVER_API_NAME &) = delete;                \
  DRIVER_API_NAME(DRIVER_API_NAME &&) = default;                               \
  DRIVER_API_NAME &operator=(DRIVER_API_NAME &&) = delete;                     \
                                                                               \
  DRIVER_API_NAME##Transaction Initiate(const char *name) {                    \
    if (nullptr == m_driver || nullptr == name)                                \
      return DRIVER_API_NAME##Transaction();                                   \
                                                                               \
    size_t length = strlen(name);                                              \
    std::unique_ptr<char[]> saved_name(new char[1 + length]);                  \
    memcpy(saved_name.get(), name, length);                                    \
    saved_name.get()[length] = '\0';                                           \
    const char *saved_name_ptr = saved_name.get();                             \
    return DRIVER_API_NAME##Transaction(m_context_sp, m_driver,                \
                                        std::move(saved_name),                 \
                                        m_driver->Initiate(saved_name_ptr));   \
  }                                                                            \
                                                                               \
  bool IsValid() { return nullptr != m_driver; }                               \
                                                                               \
private:                                                                       \
  std::shared_ptr<Context> m_context_sp;                                       \
  DRIVER_LIB_NAME *m_driver = nullptr;

#endif
