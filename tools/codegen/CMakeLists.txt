add_executable(ttrpg_codegen main.cpp)

target_link_libraries(ttrpg_codegen PRIVATE ttrpg_parse)

set_target_properties(ttrpg_codegen PROPERTIES
    CXX_CLANG_TIDY ""
)
