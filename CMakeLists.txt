cmake_minimum_required(VERSION 3.16)

project(parsec LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PARSEC_BUILD_TESTS "Build parsec unit tests" OFF)

add_subdirectory(src)

if(PARSEC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
