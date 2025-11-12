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
    src/parse.cpp
    src/ron_parser.cpp
    src/validate.cpp
  src/defaults.cpp
)

## Compiler warning flags
target_compile_options(parsec_lib PRIVATE -Wall -Wextra -Wpedantic)

# Optionally enable a stricter set of warnings and/or treat warnings as errors.
option(PARSEC_ENABLE_STRICT_WARNINGS "Enable additional strict warning flags" OFF)
option(PARSEC_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

if(PARSEC_ENABLE_STRICT_WARNINGS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(parsec_lib PRIVATE -Wconversion -Wsign-conversion -Wshadow -Wnull-dereference -Wdouble-promotion)
  elseif(MSVC)
    # MSVC specific warnings could be added here
  endif()
endif()

if(PARSEC_WARNINGS_AS_ERRORS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(parsec_lib PRIVATE -Werror)
  elseif(MSVC)
    target_compile_options(parsec_lib PRIVATE /WX)
  endif()
endif()

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
