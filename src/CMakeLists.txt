add_subdirectory(storage)
add_subdirectory(wal)
add_subdirectory(index)
add_subdirectory(buffer)
add_subdirectory(query)
add_subdirectory(concurrency)
add_subdirectory(db)

add_executable(wdb main.cpp)
target_link_libraries(wdb PRIVATE
  wdb_storage
  wdb_wal
  wdb_index
  wdb_buffer
  wdb_query
  wdb_concurrency
  wdb_db
)
