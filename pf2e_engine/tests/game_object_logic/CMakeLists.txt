add_executable(test_game_object_logic
    test_game_object_factory.cpp
)

target_link_libraries(test_game_object_logic
    gtest_main
    pf2e_engine
    nlohmann_json
)

target_include_directories(test_game_object_logic
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/game_object_logic
    PRIVATE ${CMAKE_BINARY_DIR}/cpp_config
)

include(GoogleTest)
gtest_discover_tests(test_game_object_logic)
