add_executable(test_expr
    test_expr.cpp
)

target_link_libraries(test_expr
    gtest_main
    expr
)

set_target_properties(test_expr PROPERTIES CXX_CLANG_TIDY "")

include(GoogleTest)
gtest_discover_tests(test_expr)
