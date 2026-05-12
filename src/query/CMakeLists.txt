add_library(wdb_query
  parser.cpp
  executor.cpp
)
target_include_directories(wdb_query PUBLIC ${CMAKE_SOURCE_DIR}/src)
