add_library(wdb_db
  database.cpp
)
target_include_directories(wdb_db PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(wdb_db PUBLIC
  wdb_storage wdb_wal wdb_index
)
