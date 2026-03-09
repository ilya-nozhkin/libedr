target_sources(libedr PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/ByteStream.hpp
  ${CMAKE_CURRENT_LIST_DIR}/ByteStreamAction.hpp
  ${CMAKE_CURRENT_LIST_DIR}/NamedPipe.cpp
  ${CMAKE_CURRENT_LIST_DIR}/NamedPipe.h
  ${CMAKE_CURRENT_LIST_DIR}/TCPSocket.cpp
  ${CMAKE_CURRENT_LIST_DIR}/TCPSocket.h
)
