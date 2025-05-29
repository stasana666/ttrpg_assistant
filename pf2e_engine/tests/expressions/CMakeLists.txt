add_executable(test_expressions
    test_expressions.cpp
)

target_link_libraries(test_expressions
    gtest_main
    pf2e_engine
)

target_include_directories(test_expressions
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/expressions
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/tests/test_lib
)

include(GoogleTest)
gtest_discover_tests(test_expressions)
