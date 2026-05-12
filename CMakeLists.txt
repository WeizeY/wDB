cmake_minimum_required(VERSION 3.20)
project(wDB VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type default
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# Compiler flags
add_compile_options(
  -Wall -Wextra -Wpedantic -Wno-unused-parameter
  $<$<CONFIG:Debug>:-g3>
  $<$<CONFIG:Debug>:-fsanitize=address,undefined>
  $<$<CONFIG:Release>:-O2>
  $<$<CONFIG:Release>:-DNDEBUG>
)
add_link_options(
  $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

# Source directories
add_subdirectory(src)

# Tests
enable_testing()
add_subdirectory(tests)
