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
  test_parse.cpp
  test_json.cpp
  test_ron.cpp
  test_toml.cpp
  test_ini.cpp
  test_yaml.cpp
    test_validate_unevaluated_properties.cpp
  test_validate_hs_schema_examples.cpp
  test_cli_parser_selection.cpp
  test_cli_convert.cpp
  test_pretty_print.cpp
  test_setdefaults.cpp
  test_initializer.cpp
  test_validate.cpp
  test_validate_cfd.cpp
  test_validate_medium_schema.cpp
  test_validate_medium_schema_fail.cpp
  test_validate_multi_errors.cpp
  test_anyof_defaults.cpp
  test_deprecated.cpp
  test_pq_path_parser.cpp
  test_pq_navigator.cpp
  test_pq_cli_args.cpp
  test_pq_output_formatter.cpp
)
target_link_libraries(parsec_tests PRIVATE parsec_lib Catch2::Catch2WithMain)
target_include_directories(parsec_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_compile_definitions(parsec_tests PRIVATE EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")
target_compile_definitions(parsec_tests PRIVATE PARSEC_EXE_PATH="$<TARGET_FILE:parsec>")

include(CTest)
include(Catch)
catch_discover_tests(parsec_tests)
