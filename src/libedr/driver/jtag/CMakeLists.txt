target_sources(libedr PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/Jtag.hpp
  ${CMAKE_CURRENT_LIST_DIR}/JtagAction.hpp
  ${CMAKE_CURRENT_LIST_DIR}/JtagChain.cpp
  ${CMAKE_CURRENT_LIST_DIR}/JtagChain.h
  ${CMAKE_CURRENT_LIST_DIR}/JtagChainAction.hpp
  ${CMAKE_CURRENT_LIST_DIR}/PullJtag.cpp
  ${CMAKE_CURRENT_LIST_DIR}/PullJtag.h
)
