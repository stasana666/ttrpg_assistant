add_executable(test_pf2_engine
    test_armor.cpp
    test_resources.cpp
    test_random.cpp
    test_continuation.cpp
    test_battle_map.cpp
)

target_link_libraries(test_pf2_engine
    gtest_main
    pf2e_engine
    nlohmann_json
)

target_include_directories(test_pf2_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/tests/test_lib
)

include(GoogleTest)
gtest_discover_tests(test_pf2_engine)

add_executable(test_lifecycle test_lifecycle.cpp)
target_link_libraries(test_lifecycle gtest_main pf2e_engine nlohmann_json)
gtest_discover_tests(test_lifecycle)

add_executable(test_initiative test_initiative.cpp)
target_link_libraries(test_initiative gtest_main pf2e_engine nlohmann_json)
gtest_discover_tests(test_initiative)

add_subdirectory(actions)
add_subdirectory(ast)
add_subdirectory(dsl)
add_subdirectory(expressions)
add_subdirectory(game_object_logic)
add_subdirectory(inventory)
add_subdirectory(mechanics)
add_subdirectory(transformation)
