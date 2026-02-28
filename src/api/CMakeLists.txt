set(EDR_API_HEADERS
  ${CMAKE_CURRENT_LIST_DIR}/Error.h
  ${CMAKE_CURRENT_LIST_DIR}/Context.h
  ${CMAKE_CURRENT_LIST_DIR}/Common.h
  ${CMAKE_CURRENT_LIST_DIR}/ByteStream.h
  ${CMAKE_CURRENT_LIST_DIR}/ExecutionGate.h
  ${CMAKE_CURRENT_LIST_DIR}/Jtag.h
)

set(EDR_API_SOURCES
)

set(EDR_API_C_INCLUDE_LINES)
set(EDR_API_SWIG_INCLUDE_LINES)

foreach(HEADER IN LISTS EDR_API_HEADERS)
  list(APPEND EDR_API_C_INCLUDE_LINES "#include \"${HEADER}\"")
  list(APPEND EDR_API_SWIG_INCLUDE_LINES "%include \"${HEADER}\"")
endforeach()

list(JOIN EDR_API_C_INCLUDE_LINES "\n" EDR_API_C_INCLUDES)
list(JOIN EDR_API_SWIG_INCLUDE_LINES "\n" EDR_API_SWIG_INCLUDES)

if (EDR_PYTHON3_INCLUDE_DIR)
  configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/swig/python.i.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/python.i
    @ONLY
  )

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/edr_python_api.cpp ${CMAKE_CURRENT_BINARY_DIR}/edr.py
    COMMAND ${EDR_SWIG_EXECUTABLE} -c++ -python -doxygen -o ${CMAKE_CURRENT_BINARY_DIR}/edr_python_api.cpp ${CMAKE_CURRENT_BINARY_DIR}/python.i
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/python.i ${EDR_API_HEADERS}
  )

  add_library(_edr MODULE
   ${CMAKE_CURRENT_BINARY_DIR}/edr_python_api.cpp
   ${EDR_API_HEADERS}
   ${EDR_API_SOURCES}
  )

  target_include_directories(_edr PUBLIC ${EDR_PYTHON3_INCLUDE_DIR})
  set_target_properties(_edr PROPERTIES PREFIX "")

  target_link_libraries(_edr PUBLIC libedr)

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/edr.py DESTINATION python)
  install(TARGETS _edr DESTINATION python)
endif()

configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/swig/c.i.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/c.i
  @ONLY
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/edr_c_api.cpp
  COMMAND ${EDR_SWIG_EXECUTABLE} -c++ -c -noexcept -namespace edr -o ${CMAKE_CURRENT_BINARY_DIR}/edr_c_api.cpp ${CMAKE_CURRENT_BINARY_DIR}/c.i
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/c.i ${EDR_API_HEADERS}
)

add_library(cedr SHARED
 ${CMAKE_CURRENT_BINARY_DIR}/edr_c_api.cpp
 ${EDR_API_HEADERS}
 ${EDR_API_SOURCES}
)

target_link_libraries(cedr PUBLIC libedr)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/edr_c_api.h DESTINATION include RENAME cedr.h)
install(TARGETS cedr DESTINATION lib)

set(EDR_API_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR} CACHE INTERNAL "")
