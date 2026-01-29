add_library(libedr STATIC)
set_target_properties(libedr PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(libedr PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(libedr PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../
)

add_subdirectory(driver)
