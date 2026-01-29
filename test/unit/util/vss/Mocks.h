#ifndef COMMON_VSS_MOCKS_H
#define COMMON_VSS_MOCKS_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace edr {

class VSSOutVector {
public:
  static constexpr size_t g_alignment = 4;

  template <size_t t_alignment> void Align() {
    if (0 == t_alignment)
      return;

    auto misalignment = m_data.size() & (t_alignment - 1);
    if (0 != misalignment)
      m_data.resize(m_data.size() + t_alignment - misalignment);
  }

  std::pair<size_t, void *> Allocate(size_t bytes) {
    auto origin = m_data.size();
    m_data.resize(origin + bytes);
    return {origin, &m_data[origin]};
  }

  std::vector<uint8_t> &Data() { return m_data; }

private:
  std::vector<uint8_t> m_data;
};

} // namespace edr

#endif
