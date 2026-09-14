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

add_executable(test_lifecycle_execution test_lifecycle_execution.cpp)
set(GENERATED_LIFECYCLE_HEADER ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/generated_lifecycle.h)
set(GENERATED_LIFECYCLE_SOURCE ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/generated_lifecycle.cpp)
ttrpg_generate(
    TARGET test_lifecycle_execution
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/actions/fixtures/generated_lifecycle.ttrpg
    HEADER ${GENERATED_LIFECYCLE_HEADER}
    SOURCE ${GENERATED_LIFECYCLE_SOURCE}
)
target_sources(test_lifecycle_execution PRIVATE ${GENERATED_LIFECYCLE_HEADER} ${GENERATED_LIFECYCLE_SOURCE})
target_link_libraries(test_lifecycle_execution gtest_main pf2e_engine nlohmann_json)
gtest_discover_tests(test_lifecycle_execution)

add_executable(test_initiative test_initiative.cpp)
target_link_libraries(test_initiative gtest_main pf2e_engine nlohmann_json)
gtest_discover_tests(test_initiative)

add_executable(test_knockout_initiative test_knockout_initiative.cpp)
target_link_libraries(test_knockout_initiative gtest_main pf2e_engine nlohmann_json)
gtest_discover_tests(test_knockout_initiative)

add_executable(test_health test_health.cpp)
target_link_libraries(test_health gtest_main pf2e_engine nlohmann_json)
target_include_directories(test_health PRIVATE ${CMAKE_BINARY_DIR}/cpp_config)
gtest_discover_tests(test_health)

add_subdirectory(actions)
add_subdirectory(ast)
add_subdirectory(dsl)
add_subdirectory(expressions)
add_subdirectory(game_object_logic)
add_subdirectory(inventory)
add_subdirectory(mechanics)
add_subdirectory(transformation)
