add_library(wdb_wal
  wal.cpp
)
target_include_directories(wdb_wal PUBLIC ${CMAKE_SOURCE_DIR}/src)
