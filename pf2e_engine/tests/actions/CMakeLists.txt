set(ACTION_INCLUDES ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/actions)

add_engine_test(test_actions
    SOURCES test_action_combat.cpp test_granted_combat.cpp test_mixed_attack_runtime.cpp test_legacy_damage_context.cpp
    INCLUDES ${ACTION_INCLUDES})
add_engine_test(test_wolf_combat SOURCES test_wolf_combat.cpp INCLUDES ${ACTION_INCLUDES})
add_engine_test(test_abilities SOURCES test_ability_runtime.cpp INCLUDES ${ACTION_INCLUDES})
add_engine_test(test_feature_events SOURCES test_feature_event_runtime.cpp)
add_engine_test(test_source_bindings SOURCES test_source_binding_runtime.cpp)
add_engine_test(test_world_runtime SOURCES test_world_runtime.cpp)
add_engine_test(test_movement_runtime SOURCES test_movement_runtime.cpp)
add_engine_test(test_reaction_runtime SOURCES test_reaction_runtime.cpp)
add_engine_test(test_action_history SOURCES test_action_history.cpp)
add_engine_test(test_time_relations SOURCES test_time_relations.cpp)
add_engine_test(test_generated_movement SOURCES test_generated_movement.cpp)
add_engine_test(test_generated_shield SOURCES test_generated_shield.cpp)
add_engine_test(test_generated_trip SOURCES test_generated_trip.cpp)
add_engine_test(test_generated_intimidation SOURCES test_generated_intimidation.cpp)
add_engine_test(test_generated_encounter SOURCES test_generated_encounter.cpp)
add_engine_test(test_generated_flanking SOURCES test_generated_flanking.cpp)

foreach(FIXTURE IN ITEMS actions sources world flow activities source_choices reactions records relations)
    add_engine_test(test_generated_${FIXTURE}
        SOURCES test_generated_${FIXTURE}.cpp
        SCHEMAS fixtures/generated_${FIXTURE}.ttrpg)
endforeach()
add_engine_test(test_generated_strike
    SOURCES test_generated_strike.cpp
    SCHEMAS fixtures/generated_strike_activities.ttrpg)
