target_sources(libedr PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/HeavyTemplates.cpp
)

add_subdirectory(apb)
add_subdirectory(bytestream)
add_subdirectory(execution_gate)
add_subdirectory(jtag)
add_subdirectory(riscv)
