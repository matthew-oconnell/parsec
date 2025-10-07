cmake_minimum_required(VERSION 3.16)

project(parsec-lib LANGUAGES CXX)

add_library(parsec_lib STATIC)

target_compile_features(parsec_lib PUBLIC cxx_std_17)

target_include_directories(parsec_lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_sources(parsec_lib
  PRIVATE
    src/parsec.cpp
    src/json_parser.cpp
    src/ron_parser.cpp
)

target_compile_options(parsec_lib PRIVATE -Wall -Wextra -Wpedantic)

option(PARSEC_BUILD_CLI "Build parsec CLI tool" ON)
if (PARSEC_BUILD_CLI)
  add_executable(parsec src/main.cpp)
  target_link_libraries(parsec PRIVATE parsec_lib)
  target_include_directories(parsec PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_features(parsec PUBLIC cxx_std_17)
  target_compile_options(parsec PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (PARSEC_BUILD_TESTS)
  # tests are handled at top-level in test/
endif()
