target_sources(pf2e_engine
    PRIVATE
        battle_map.cpp
        battle.cpp
        combat_calculator.cpp
        condition.cpp
        creature.cpp
        effect_manager.cpp
        initiative_order.cpp
        interaction_system.cpp
        proficiency.cpp
        random.cpp
        resources.cpp
        success_level.cpp
        scheduler.cpp
        weapon_slot.cpp
)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine
    PUBLIC ${CMAKE_SOURCE_DIR}/pf2e_engine/include
)

add_subdirectory(action_blocks)
add_subdirectory(actions)
add_subdirectory(common)
add_subdirectory(expressions)
add_subdirectory(game_object_logic)
add_subdirectory(inventory)
add_subdirectory(mechanics)
add_subdirectory(transformation)
