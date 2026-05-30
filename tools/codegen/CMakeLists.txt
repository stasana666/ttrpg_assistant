add_executable(ttrpg_codegen main.cpp)

set_target_properties(ttrpg_codegen PROPERTIES
    CXX_CLANG_TIDY ""
)
