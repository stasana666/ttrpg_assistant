add_executable(test_ast_state
    test_ast_state.cpp
)

target_link_libraries(test_ast_state
    gtest_main
    pf2e_engine
)

target_include_directories(test_ast_state
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/tests/test_lib
)

include(GoogleTest)
gtest_discover_tests(test_ast_state)
