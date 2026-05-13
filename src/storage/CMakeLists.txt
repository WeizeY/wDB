add_library(wdb_storage
  page.cpp
  record.cpp
)
target_include_directories(wdb_storage PUBLIC ${CMAKE_SOURCE_DIR}/src)
