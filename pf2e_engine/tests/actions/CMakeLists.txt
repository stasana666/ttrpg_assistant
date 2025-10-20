add_executable(test_action
    test_action.cpp
)

target_link_libraries(test_action
    gtest_main
    pf2e_engine
    nlohmann_json
)

target_include_directories(test_action
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/actions
    PRIVATE ${CMAKE_BINARY_DIR}/cpp_config
)

include(GoogleTest)
gtest_discover_tests(test_action)
