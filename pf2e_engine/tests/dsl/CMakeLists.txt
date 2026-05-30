add_executable(test_dsl
    test_dsl_parser.cpp
    test_dsl_evaluator.cpp
)

target_link_libraries(test_dsl
    gtest_main
    pf2e_engine
)

target_include_directories(test_dsl
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/tests/test_lib
)

include(GoogleTest)
gtest_discover_tests(test_dsl)
