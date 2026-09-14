add_executable(test_actions
    test_action_combat.cpp
    test_granted_combat.cpp
    test_mixed_attack_runtime.cpp
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

include(GoogleTest)
gtest_discover_tests(test_actions)

add_executable(test_generated_strike
    test_generated_strike.cpp
)

target_link_libraries(test_generated_strike
    gtest_main
    pf2e_engine
)

target_include_directories(test_generated_strike
    PRIVATE ${CMAKE_BINARY_DIR}/cpp_config
)

gtest_discover_tests(test_generated_strike)

add_executable(test_wolf_combat
    test_wolf_combat.cpp
)

target_link_libraries(test_wolf_combat
    gtest_main
    pf2e_engine
    nlohmann_json
)

target_include_directories(test_wolf_combat
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/actions
    PRIVATE ${CMAKE_BINARY_DIR}/cpp_config
)

gtest_discover_tests(test_wolf_combat)

add_executable(test_abilities
    test_ability_runtime.cpp
)

target_link_libraries(test_abilities
    gtest_main
    pf2e_engine
    nlohmann_json
)

target_include_directories(test_abilities
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/actions
    PRIVATE ${CMAKE_BINARY_DIR}/cpp_config
)

gtest_discover_tests(test_abilities)

add_executable(test_generated_actions
    test_generated_actions.cpp
)

set(GENERATED_ACTIONS_HEADER ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/generated_actions.h)
set(GENERATED_ACTIONS_SOURCE ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/generated_actions.cpp)
ttrpg_generate(
    TARGET test_generated_actions
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/generated_actions.ttrpg
    HEADER ${GENERATED_ACTIONS_HEADER}
    SOURCE ${GENERATED_ACTIONS_SOURCE}
)
target_sources(test_generated_actions PRIVATE ${GENERATED_ACTIONS_HEADER} ${GENERATED_ACTIONS_SOURCE})

target_link_libraries(test_generated_actions
    gtest_main
    pf2e_engine
)

gtest_discover_tests(test_generated_actions)

add_executable(test_generated_sources
    test_generated_sources.cpp
)

set(GENERATED_SOURCES_HEADER ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/generated_sources.h)
set(GENERATED_SOURCES_SOURCE ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/generated_sources.cpp)
ttrpg_generate(
    TARGET test_generated_sources
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/generated_sources.ttrpg
    HEADER ${GENERATED_SOURCES_HEADER}
    SOURCE ${GENERATED_SOURCES_SOURCE}
)
target_sources(test_generated_sources PRIVATE ${GENERATED_SOURCES_HEADER} ${GENERATED_SOURCES_SOURCE})

target_link_libraries(test_generated_sources
    gtest_main
    pf2e_engine
)

gtest_discover_tests(test_generated_sources)

add_executable(test_generated_world
    test_generated_world.cpp
)

set(GENERATED_WORLD_HEADER ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/generated_world.h)
set(GENERATED_WORLD_SOURCE ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/generated_world.cpp)
ttrpg_generate(
    TARGET test_generated_world
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/generated_world.ttrpg
    HEADER ${GENERATED_WORLD_HEADER}
    SOURCE ${GENERATED_WORLD_SOURCE}
)
target_sources(test_generated_world PRIVATE ${GENERATED_WORLD_HEADER} ${GENERATED_WORLD_SOURCE})

target_link_libraries(test_generated_world
    gtest_main
    pf2e_engine
)

gtest_discover_tests(test_generated_world)

add_executable(test_feature_events
    test_feature_event_runtime.cpp
)

target_link_libraries(test_feature_events
    gtest_main
    pf2e_engine
)

gtest_discover_tests(test_feature_events)

add_executable(test_source_bindings
    test_source_binding_runtime.cpp
)

target_link_libraries(test_source_bindings
    gtest_main
    pf2e_engine
)

gtest_discover_tests(test_source_bindings)

add_executable(test_world_runtime
    test_world_runtime.cpp
)

target_link_libraries(test_world_runtime
    gtest_main
    pf2e_engine
)

gtest_discover_tests(test_world_runtime)
