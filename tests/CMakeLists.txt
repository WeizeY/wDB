find_package(GTest REQUIRED)
include(GoogleTest)

function(wdb_add_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE
    GTest::gtest GTest::gtest_main
    ${ARGN}
  )
  target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/tests)
  gtest_discover_tests(${name})
endfunction()

wdb_add_test(test_record        wdb_storage)
wdb_add_test(test_page          wdb_storage)
wdb_add_test(test_file_manager  wdb_storage)
wdb_add_test(test_heap_storage  wdb_storage)
wdb_add_test(test_wal           wdb_wal)
wdb_add_test(test_btree_node    wdb_index wdb_storage)
wdb_add_test(test_database      wdb_db)
