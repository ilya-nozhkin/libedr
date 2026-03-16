add_library(libedr STATIC)
set_target_properties(libedr PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(libedr PROPERTIES POSITION_INDEPENDENT_CODE ON)

set_target_properties(libedr PROPERTIES PREFIX "")

target_include_directories(libedr PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../
)

add_subdirectory(driver)

install(TARGETS libedr DESTINATION lib)
install(
  DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  DESTINATION include
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "*.hpp"
)
