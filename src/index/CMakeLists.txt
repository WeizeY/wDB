add_library(wdb_index
  btree_node.cpp
  btree.cpp
)
target_include_directories(wdb_index PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(wdb_index PUBLIC wdb_storage)
