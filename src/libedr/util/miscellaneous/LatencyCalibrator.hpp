#ifndef LIBEDR_UTIL_MISCELLANEOUS_LATENCY_HPP
#define LIBEDR_UTIL_MISCELLANEOUS_LATENCY_HPP

#include <algorithm>
#include <cstdint>

namespace edr {

class LatencyCalibrator final {
public:
  LatencyCalibrator(uint32_t min_latency, uint32_t max_latency)
      : m_min_latency(std::min(min_latency, max_latency)),
        m_max_latency(std::max(min_latency, max_latency)),
        m_current_latency(min_latency) {}

  void BumpMin(uint32_t min_min) {
    m_min_latency = std::max(min_min, m_min_latency);
    m_current_latency = std::max(min_min, m_current_latency);
    m_max_latency = std::max(min_min, m_max_latency);
  }

  void TooLow() {
    m_min_latency = m_current_latency + 1;

    auto room = m_max_latency - m_current_latency;
    if (room <= m_current_latency) {
      m_current_latency = m_max_latency;
      return;
    }

    m_current_latency = m_current_latency * 2 + 1;
  }

  void Enough() {
    auto step = (m_current_latency - m_min_latency + 1) / 2;
    m_current_latency -= step;
  }

  uint32_t Get() { return m_current_latency; }

  bool IsMax() { return m_current_latency == m_max_latency; }

private:
  uint32_t m_min_latency;
  uint32_t m_max_latency;
  uint32_t m_current_latency;
};

} // namespace edr

#endif
