target_sources(pf2e_engine
    PRIVATE
        armor_class.cpp
        armor_slot.cpp
        weapon_slot.cpp
        creature.cpp
        resources.cpp
        weapon_damage_factory.cpp
)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine
)

add_subdirectory(inventory)
add_subdirectory(mechanics)
add_subdirectory(dice_expression)
