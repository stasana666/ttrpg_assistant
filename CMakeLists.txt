cmake_minimum_required(VERSION 3.15)
project(TTRPG_ASSISTANT LANGUAGES CXX)

enable_testing()

#set(CMAKE_CXX_CLANG_TIDY "clang-tidy")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cpp_config/cpp_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/cpp_config/cpp_config.h)

add_subdirectory(pf2e_engine)
