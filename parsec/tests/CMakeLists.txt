cmake_minimum_required(VERSION 3.16)

include(FetchContent)

FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(catch2)

add_executable(parsec_tests test_parsec.cpp)
target_link_libraries(parsec_tests PRIVATE parsec Catch2::Catch2WithMain)
target_include_directories(parsec_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

include(CTest)
include(Catch)
catch_discover_tests(parsec_tests)
