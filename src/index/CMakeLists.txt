add_library(wdb_index
  btree.cpp
)
target_include_directories(wdb_index PUBLIC ${CMAKE_SOURCE_DIR}/src)
