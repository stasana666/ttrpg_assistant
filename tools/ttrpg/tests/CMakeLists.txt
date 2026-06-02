add_executable(test_ttrpg
    test_writer.cpp
    test_conventions.cpp
    test_parser.cpp
    test_golden.cpp
)

target_link_libraries(test_ttrpg
    gtest_main
    ttrpg
)

# test_parser / test_golden read .ttrpg fixtures + golden output from this dir.
target_compile_definitions(test_ttrpg PRIVATE
    TTRPG_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

# The language tooling opts out of clang-tidy; its tests do too.
set_target_properties(test_ttrpg PROPERTIES CXX_CLANG_TIDY "")

include(GoogleTest)
gtest_discover_tests(test_ttrpg)
