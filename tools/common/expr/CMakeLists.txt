# Shared expression front-end: lexer + parser producing a backend-agnostic AST
# (expr::TExprNode). Consumed by the `.ttrpg` code generator (lowers to C++) and
# by the engine's runtime DSL (evaluates). Built as a static lib so both can
# link it; PIC because it links into the shared libpf2e_engine.
add_library(expr STATIC
    src/lexer.cpp
    src/parser.cpp
)

target_include_directories(expr PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(expr PUBLIC ttrpg_parse)

# Same opt-out as the rest of the language tooling (camelCase-free, but the
# string-building/parsing style trips the project tidy config); PIC so it can be
# linked into the shared engine library.
set_target_properties(expr PROPERTIES
    CXX_CLANG_TIDY ""
    POSITION_INDEPENDENT_CODE ON
)

add_subdirectory(tests)
