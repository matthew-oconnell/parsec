cmake_minimum_required(VERSION 3.16)

include(FetchContent)

FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(catch2)

add_executable(parsec_tests
  test_parsec.cpp
  test_dictionary.cpp
  test_parse_examples.cpp
  test_comments.cpp
  test_parse_errors.cpp
  test_ron.cpp
  test_cli_parser_selection.cpp
  test_pretty_print.cpp
  test_validate.cpp
  test_validate_cfd.cpp
  test_validate_phase1.cpp
  test_validate_phase2.cpp
)
target_link_libraries(parsec_tests PRIVATE parsec_lib Catch2::Catch2WithMain)
target_include_directories(parsec_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_compile_definitions(parsec_tests PRIVATE EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")

include(CTest)
include(Catch)
catch_discover_tests(parsec_tests)
