add_executable(test_actions
    test_action_combat.cpp
    test_granted_combat.cpp
    test_mixed_attack_runtime.cpp
    test_legacy_damage_context.cpp
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

set(GENERATED_STRIKE_ACTIVITIES_HEADER ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/generated_strike_activities.h)
set(GENERATED_STRIKE_ACTIVITIES_SOURCE ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/generated_strike_activities.cpp)
ttrpg_generate(
    TARGET test_generated_strike
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/generated_strike_activities.ttrpg
    HEADER ${GENERATED_STRIKE_ACTIVITIES_HEADER}
    SOURCE ${GENERATED_STRIKE_ACTIVITIES_SOURCE}
)
target_sources(test_generated_strike PRIVATE ${GENERATED_STRIKE_ACTIVITIES_HEADER} ${GENERATED_STRIKE_ACTIVITIES_SOURCE})

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

add_executable(test_generated_flow
    test_generated_flow.cpp
)

set(GENERATED_FLOW_HEADER ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/generated_flow.h)
set(GENERATED_FLOW_SOURCE ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/generated_flow.cpp)
ttrpg_generate(
    TARGET test_generated_flow
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/generated_flow.ttrpg
    HEADER ${GENERATED_FLOW_HEADER}
    SOURCE ${GENERATED_FLOW_SOURCE}
)
target_sources(test_generated_flow PRIVATE ${GENERATED_FLOW_HEADER} ${GENERATED_FLOW_SOURCE})

target_link_libraries(test_generated_flow
    gtest_main
    pf2e_engine
)

gtest_discover_tests(test_generated_flow)

add_executable(test_movement_runtime
    test_movement_runtime.cpp
)

target_link_libraries(test_movement_runtime
    gtest_main
    pf2e_engine
)

gtest_discover_tests(test_movement_runtime)

add_executable(test_generated_activities
    test_generated_activities.cpp
)

set(GENERATED_ACTIVITIES_HEADER ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/generated_activities.h)
set(GENERATED_ACTIVITIES_SOURCE ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/generated_activities.cpp)
ttrpg_generate(
    TARGET test_generated_activities
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/generated_activities.ttrpg
    HEADER ${GENERATED_ACTIVITIES_HEADER}
    SOURCE ${GENERATED_ACTIVITIES_SOURCE}
)
target_sources(test_generated_activities PRIVATE ${GENERATED_ACTIVITIES_HEADER} ${GENERATED_ACTIVITIES_SOURCE})

target_link_libraries(test_generated_activities
    gtest_main
    pf2e_engine
)

gtest_discover_tests(test_generated_activities)

add_executable(test_generated_source_choices
    test_generated_source_choices.cpp
)

set(GENERATED_SOURCE_CHOICES_HEADER ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/generated_source_choices.h)
set(GENERATED_SOURCE_CHOICES_SOURCE ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/generated_source_choices.cpp)
ttrpg_generate(
    TARGET test_generated_source_choices
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/generated_source_choices.ttrpg
    HEADER ${GENERATED_SOURCE_CHOICES_HEADER}
    SOURCE ${GENERATED_SOURCE_CHOICES_SOURCE}
)
target_sources(test_generated_source_choices PRIVATE ${GENERATED_SOURCE_CHOICES_HEADER} ${GENERATED_SOURCE_CHOICES_SOURCE})

target_link_libraries(test_generated_source_choices
    gtest_main
    pf2e_engine
)

gtest_discover_tests(test_generated_source_choices)

add_executable(test_reaction_runtime test_reaction_runtime.cpp)
target_link_libraries(test_reaction_runtime gtest_main pf2e_engine)
gtest_discover_tests(test_reaction_runtime)

add_executable(test_generated_reactions test_generated_reactions.cpp)

set(GENERATED_REACTIONS_HEADER ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/generated_reactions.h)
set(GENERATED_REACTIONS_SOURCE ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/generated_reactions.cpp)
ttrpg_generate(
    TARGET test_generated_reactions
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/generated_reactions.ttrpg
    HEADER ${GENERATED_REACTIONS_HEADER}
    SOURCE ${GENERATED_REACTIONS_SOURCE}
)
target_sources(test_generated_reactions PRIVATE ${GENERATED_REACTIONS_HEADER} ${GENERATED_REACTIONS_SOURCE})
target_link_libraries(test_generated_reactions gtest_main pf2e_engine)
gtest_discover_tests(test_generated_reactions)

add_executable(test_generated_movement test_generated_movement.cpp)
target_link_libraries(test_generated_movement gtest_main pf2e_engine)
gtest_discover_tests(test_generated_movement)

add_executable(test_generated_shield test_generated_shield.cpp)
target_link_libraries(test_generated_shield gtest_main pf2e_engine)
gtest_discover_tests(test_generated_shield)

add_executable(test_action_history test_action_history.cpp)
target_link_libraries(test_action_history gtest_main pf2e_engine)
gtest_discover_tests(test_action_history)

add_executable(test_generated_records test_generated_records.cpp)

set(GENERATED_RECORDS_HEADER ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/generated_records.h)
set(GENERATED_RECORDS_SOURCE ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/generated_records.cpp)
ttrpg_generate(
    TARGET test_generated_records
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/generated_records.ttrpg
    HEADER ${GENERATED_RECORDS_HEADER}
    SOURCE ${GENERATED_RECORDS_SOURCE}
)
target_sources(test_generated_records PRIVATE ${GENERATED_RECORDS_HEADER} ${GENERATED_RECORDS_SOURCE})
target_link_libraries(test_generated_records gtest_main pf2e_engine)
gtest_discover_tests(test_generated_records)
