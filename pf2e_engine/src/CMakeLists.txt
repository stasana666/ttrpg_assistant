target_sources(pf2e_engine
    PRIVATE
        alternatives.cpp
        battle_map.cpp
        battle.cpp
        rules.cpp
        initiative_order.cpp
        i_interaction_system.cpp
        player.cpp
        random.cpp
)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine
    PUBLIC ${CMAKE_SOURCE_DIR}/pf2e_engine/include
)

add_subdirectory(actions)
add_subdirectory(common)
add_subdirectory(inventory)
add_subdirectory(transformation)
