add_subdirectory(storage)
add_subdirectory(wal)
add_subdirectory(index)
add_subdirectory(db)

# Future phases activate their own subdirectories:
#   add_subdirectory(buffer)       # Phase 4 (buffer pool)
#   add_subdirectory(query)        # Phase 5 (parser/executor)
#   add_subdirectory(concurrency)  # Phase 6 (locks/threadpool)

add_executable(wdb main.cpp)
target_link_libraries(wdb PRIVATE
  wdb_storage
  wdb_wal
  wdb_index
  wdb_db
)
