cmake_minimum_required(VERSION 3.16)

project(parsec-lib LANGUAGES CXX)

add_library(parsec STATIC)

target_compile_features(parsec PUBLIC cxx_std_17)

target_include_directories(parsec
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
		$<INSTALL_INTERFACE:include>
)

target_sources(parsec
	PRIVATE
		src/parsec.cpp
)

target_compile_options(parsec PRIVATE -Wall -Wextra -Wpedantic)

if (PARSEC_BUILD_TESTS)
	add_subdirectory(tests)
endif()
