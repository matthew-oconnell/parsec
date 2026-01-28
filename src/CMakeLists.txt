cmake_minimum_required(VERSION 3.16)

project(parsec-lib LANGUAGES CXX)

add_library(parsec_lib STATIC)
add_library(ps::parsec ALIAS parsec_lib)

target_compile_features(parsec_lib PUBLIC cxx_std_17)

target_include_directories(parsec_lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_sources(parsec_lib
  PRIVATE
    src/defaults.cpp
    src/dictionary.cpp
    src/json_parser.cpp
    src/parse.cpp
    src/parsec.cpp
    src/ron_parser.cpp
    src/toml_parser.cpp
    src/toml_printer.cpp
    src/ini_parser.cpp
    src/yaml_parser.cpp
    src/yaml_printer.cpp
    src/ron_printer.cpp
    src/validate.cpp
    src/pq/path_parser.cpp
    src/pq/navigator.cpp
    src/pq/cli_args.cpp
)

## Compiler warning flags
target_compile_options(parsec_lib PRIVATE -Wall -Wextra -Wpedantic)

# Optionally enable a stricter set of warnings and/or treat warnings as errors.
option(PARSEC_ENABLE_STRICT_WARNINGS "Enable additional strict warning flags" OFF)
option(PARSEC_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

if(PARSEC_ENABLE_STRICT_WARNINGS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(parsec_lib PRIVATE
      -Wconversion -Wsign-conversion -Wshadow -Wnull-dereference -Wdouble-promotion
    )
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
  target_include_directories(parsec_lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> 
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>       
  )
  target_compile_features(parsec PUBLIC cxx_std_17)
  target_compile_options(parsec PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (PARSEC_BUILD_TESTING)
  # tests are handled at top-level in test/
  # Build test utility for bash integration tests
  add_executable(pq_test_parser src/pq/test_parser_main.cpp)
  target_link_libraries(pq_test_parser PRIVATE parsec_lib)
  target_compile_features(pq_test_parser PUBLIC cxx_std_17)
  target_compile_options(pq_test_parser PRIVATE -Wall -Wextra -Wpedantic)
endif()

# -------------------------------
# Install & export parsec targets
# -------------------------------
include(GNUInstallDirs)

install(TARGETS parsec_lib
  EXPORT parsecTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if (PARSEC_BUILD_CLI)
  install(TARGETS parsec
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()

install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT parsecTargets
  NAMESPACE ps::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/parsec
)