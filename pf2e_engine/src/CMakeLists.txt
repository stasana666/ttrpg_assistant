target_sources(pf2e_engine
    PRIVATE
        armor_slot.cpp
        creature.cpp
        game_object_storage.cpp
        random.cpp
        resources.cpp
        weapon_damage_factory.cpp
        weapon_slot.cpp
)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine
    PUBLIC ${CMAKE_SOURCE_DIR}/pf2e_engine/include
)

add_subdirectory(common)
add_subdirectory(expressions)
add_subdirectory(inventory)
add_subdirectory(mechanics)
