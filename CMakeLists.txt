cmake_minimum_required(VERSION 3.15)
project(TTRPG_ASSISTANT LANGUAGES CXX)

if(POLICY CMP0118)
    cmake_policy(SET CMP0118 NEW)
endif()

enable_testing()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(extern)

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -g
)

set(CMAKE_CXX_CLANG_TIDY "clang-tidy")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cpp_config/cpp_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/cpp_config/cpp_config.h)

add_subdirectory(tools/codegen)

add_subdirectory(pf2e_engine)
add_subdirectory(assistant)
add_subdirectory(analyzer)
