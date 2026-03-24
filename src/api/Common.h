#ifndef LIBEDR_API_COMMON_H
#define LIBEDR_API_COMMON_H

#include <memory>

#ifndef SWIG_PYTHON_THREAD_BEGIN_ALLOW
#define SWIG_PYTHON_THREAD_BEGIN_ALLOW
#endif

#ifndef SWIG_PYTHON_THREAD_END_ALLOW
#define SWIG_PYTHON_THREAD_END_ALLOW
#endif

#define TRANSACTION_BODY(DRIVER_NAME)                                          \
  friend class DRIVER_NAME;                                                    \
                                                                               \
public:                                                                        \
  DRIVER_NAME##Transaction(const DRIVER_NAME##Transaction &) = delete;         \
  DRIVER_NAME##Transaction &operator=(const DRIVER_NAME##Transaction &) =      \
      delete;                                                                  \
  DRIVER_NAME##Transaction(DRIVER_NAME##Transaction &&) = default;             \
  DRIVER_NAME##Transaction &operator=(DRIVER_NAME##Transaction &&) = delete;   \
                                                                               \
  bool Schedule() {                                                            \
    if (!m_builder)                                                            \
      return false;                                                            \
                                                                               \
    m_task.emplace(m_context_sp->GetNameGuard().Schedule(                      \
        m_name_cookie, *m_driver, std::move(*m_builder)));                     \
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
    SWIG_PYTHON_THREAD_BEGIN_ALLOW;                                            \
    m_driver->Join(*m_task);                                                   \
    SWIG_PYTHON_THREAD_END_ALLOW;                                              \
  }                                                                            \
                                                                               \
  void Do() {                                                                  \
    Schedule();                                                                \
    Join();                                                                    \
  }                                                                            \
                                                                               \
  void Next() {                                                                \
    if (!InitCheckIterator())                                                  \
      return;                                                                  \
                                                                               \
    ++(*m_iterator);                                                           \
  }                                                                            \
                                                                               \
  void NextN(unsigned num) {                                                   \
    for (unsigned i = 0; i < num; i++)                                         \
      Next();                                                                  \
  }                                                                            \
                                                                               \
  void Reuse(const char *name) {                                               \
    auto [new_cookie, persistent_name] =                                       \
        m_context_sp->GetNameGuard().AllocateName(name);                       \
                                                                               \
    m_name_cookie = new_cookie;                                                \
                                                                               \
    if (m_task && *m_task && m_task->done()) {                                 \
      m_builder.emplace(                                                       \
          m_driver->Initiate(persistent_name, std::move(**m_task)));           \
      m_task.reset();                                                          \
      return;                                                                  \
    }                                                                          \
                                                                               \
    m_builder.emplace(m_driver->Initiate(persistent_name));                    \
    m_task.reset();                                                            \
  }                                                                            \
                                                                               \
private:                                                                       \
  DRIVER_NAME##Transaction(const std::shared_ptr<Context> &context_sp,         \
                           edr::DRIVER_NAME *driver,                           \
                           TransactionNameGuard::Cookie name_cookie,           \
                           edr::DRIVER_NAME::Builder &&builder)                \
      : m_context_sp(context_sp), m_driver(driver),                            \
        m_name_cookie(name_cookie), m_builder(std::move(builder)) {}           \
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
  DRIVER_NAME##Transaction() = default;                                        \
                                                                               \
  std::shared_ptr<Context> m_context_sp;                                       \
  edr::DRIVER_NAME *m_driver = nullptr;                                        \
  TransactionNameGuard::Cookie m_name_cookie;                                  \
  std::string m_error_message;                                                 \
  std::optional<edr::DRIVER_NAME::Builder> m_builder;                          \
  std::optional<TransactionNameGuard::Task<edr::DRIVER_NAME::Status>> m_task;  \
  std::optional<edr::DRIVER_NAME::Status::Iterator> m_iterator;

#ifdef SWIG
#define EXTERNAL_DRIVER_CONSTRUCTOR(DRIVER_NAME)
#define SELF(DRIVER_NAME)
#define MOVE_CONSTRUCTOR(DRIVER_NAME) DRIVER_NAME(DRIVER_NAME &&) = delete;
#else
#define EXTERNAL_DRIVER_CONSTRUCTOR(DRIVER_NAME)                               \
  DRIVER_NAME(const std::shared_ptr<Context> &context_sp,                      \
              edr::DRIVER_NAME *driver = nullptr)                              \
      : DriverBase(context_sp, driver) {}

#define SELF(DRIVER_NAME)                                                      \
  edr::DRIVER_NAME *Self() { return static_cast<edr::DRIVER_NAME *>(m_driver); }

#define MOVE_CONSTRUCTOR(DRIVER_NAME) DRIVER_NAME(DRIVER_NAME &&) = default;
#endif

#define DRIVER_BODY(DRIVER_NAME)                                               \
public:                                                                        \
  EXTERNAL_DRIVER_CONSTRUCTOR(DRIVER_NAME)                                     \
                                                                               \
  DRIVER_NAME(const DRIVER_NAME &) = delete;                                   \
  DRIVER_NAME &operator=(const DRIVER_NAME &) = delete;                        \
  DRIVER_NAME &operator=(DRIVER_NAME &&) = delete;                             \
  MOVE_CONSTRUCTOR(DRIVER_NAME)                                                \
                                                                               \
  ~DRIVER_NAME() override = default;                                           \
                                                                               \
  DRIVER_NAME##Transaction Initiate(const char *name) {                        \
    if (nullptr == m_driver || nullptr == name)                                \
      return DRIVER_NAME##Transaction();                                       \
                                                                               \
    auto [name_cookie, persistent_name] =                                      \
        m_context_sp->GetNameGuard().AllocateName(name);                       \
    return DRIVER_NAME##Transaction(m_context_sp, Self(), name_cookie,         \
                                    Self()->Initiate(persistent_name));        \
  }                                                                            \
                                                                               \
  SELF(DRIVER_NAME)                                                            \
                                                                               \
  void Terminate() { Self()->Terminate(); }                                    \

#endif
