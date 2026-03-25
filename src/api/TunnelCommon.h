#ifndef LIBEDR_API_TUNNELCOMMON_H
#define LIBEDR_API_TUNNELCOMMON_H

#define ALL_DRIVERS(DO)                                                        \
  DO(APB)                                                               \
  DO(ByteStream)                                                               \
  DO(ExecutionGate)                                                            \
  DO(Jtag)

#define FIND_DRIVER_FUNCTION(DRIVER_NAME)                                      \
  DRIVER_NAME Find##DRIVER_NAME(const char *name) {                            \
    return DRIVER_NAME(m_context_sp,                                           \
                       m_tunnel.FindByName<edr::DRIVER_NAME>(name));           \
  }

#define TUNNEL_BODY                                                            \
public:                                                                        \
  ALL_DRIVERS(FIND_DRIVER_FUNCTION)

#endif
