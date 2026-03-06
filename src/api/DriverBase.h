#ifndef LIBEDR_API_DRIVERBASE_H
#define LIBEDR_API_DRIVERBASE_H

#include "api/Context.h"
#include "libedr/driver/Driver.hpp"

class DriverBase {
public:
#ifdef SWIG
  DriverBase() = delete;
#else
  DriverBase(std::shared_ptr<Context> context_sp,
             edr::DriverBase *driver = nullptr)
      : m_context_sp(context_sp), m_driver(driver) {}

  edr::DriverBase *Base() { return m_driver; }
#endif

  bool IsValid() { return nullptr != m_driver; }

protected:
  std::shared_ptr<Context> m_context_sp;
  edr::DriverBase *m_driver = nullptr;
};

#endif
