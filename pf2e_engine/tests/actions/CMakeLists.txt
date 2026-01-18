add_executable(test_actions
    test_action_combat.cpp
)

target_link_libraries(test_actions
    gtest_main
    pf2e_engine
    nlohmann_json
)

target_include_directories(test_actions
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/actions
    PRIVATE ${CMAKE_BINARY_DIR}/cpp_config
)
