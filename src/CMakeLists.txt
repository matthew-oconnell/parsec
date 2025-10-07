cmake_minimum_required(VERSION 3.16)

project(parsec-lib LANGUAGES CXX)

add_library(parsec STATIC)

target_compile_features(parsec PUBLIC cxx_std_17)

target_include_directories(parsec
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_sources(parsec
  PRIVATE
    src/parsec.cpp
    src/json_parser.cpp
)

target_compile_options(parsec PRIVATE -Wall -Wextra -Wpedantic)

option(PARSEC_BUILD_CLI "Build parsec CLI tool" ON)
if (PARSEC_BUILD_CLI)
  add_executable(parsec_cli src/main.cpp)
  target_link_libraries(parsec_cli PRIVATE parsec)
  target_include_directories(parsec_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_features(parsec_cli PUBLIC cxx_std_17)
  target_compile_options(parsec_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (PARSEC_BUILD_TESTS)
  # tests are handled at top-level in test/
endif()
