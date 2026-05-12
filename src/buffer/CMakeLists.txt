add_library(wdb_buffer
  buffer_pool.cpp
)
target_include_directories(wdb_buffer PUBLIC ${CMAKE_SOURCE_DIR}/src)
