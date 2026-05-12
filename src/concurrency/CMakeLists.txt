add_library(wdb_concurrency
  locks.cpp
  thread_pool.cpp
)
target_include_directories(wdb_concurrency PUBLIC ${CMAKE_SOURCE_DIR}/src)
