cmake_minimum_required(VERSION 3.20)
project(wDB VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WDB_COVERAGE "Build with gcov coverage instrumentation" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

if(WDB_COVERAGE)
  # Coverage builds: -O0, debug info, gcov flags. Disable sanitizers (interfere
  # with coverage on some toolchains and slow runs needlessly).
  add_compile_options(-O0 -g --coverage)
  add_link_options(--coverage)
else()
  add_compile_options(
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:Release>:-DNDEBUG>
  )
  add_link_options(
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
  )
endif()

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)
