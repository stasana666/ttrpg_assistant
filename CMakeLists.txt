cmake_minimum_required(VERSION 3.15)
project(TTRPG_ASSISTANT LANGUAGES CXX)

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

set(CMAKE_CXX_CLANG_TIDY "/opt/homebrew/opt/llvm/bin/clang-tidy")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cpp_config/cpp_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/cpp_config/cpp_config.h)

add_subdirectory(pf2e_engine)
