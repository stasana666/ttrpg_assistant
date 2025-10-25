target_sources(pf2e_engine
    PRIVATE
        battle_map.cpp
        creature.cpp
        combat_calculator.cpp
        random.cpp
        resources.cpp
        success_level.cpp
        weapon_slot.cpp
)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine
    PUBLIC ${CMAKE_SOURCE_DIR}/pf2e_engine/include
)

add_subdirectory(actions)
add_subdirectory(action_blocks)
add_subdirectory(common)
add_subdirectory(expressions)
add_subdirectory(game_object_logic)
add_subdirectory(inventory)
add_subdirectory(mechanics)
add_subdirectory(transformation)
