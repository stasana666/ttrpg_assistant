# The `.ttrpg` schema language: lexer/parser/module-loader (frontend),
# conventions (semantic layer), and C++ emission (backend). Built as a static
# library so it can be unit/golden-tested; `ttrpg_codegen` is a thin CLI over it.

add_library(ttrpg STATIC
    src/parser.cpp
    src/module.cpp
    src/conventions.cpp
    src/analyze.cpp
    src/program.cpp
    src/abilities.cpp
    src/cpp_writer.cpp
    src/emit.cpp
    src/emit_action.cpp
    src/emit_rule_class.cpp
)

target_include_directories(ttrpg PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ttrpg PUBLIC ttrpg_parse expr)

# clang-tidy is disabled on the language tooling (camelCase locals, generated
# string templates) -- same policy the old ttrpg_codegen target used.
set_target_properties(ttrpg PROPERTIES CXX_CLANG_TIDY "")

add_executable(ttrpg_codegen cli/main.cpp)
target_link_libraries(ttrpg_codegen PRIVATE ttrpg)
set_target_properties(ttrpg_codegen PROPERTIES CXX_CLANG_TIDY "")

include(cmake/TtrpgCodegen.cmake)

add_subdirectory(tests)
