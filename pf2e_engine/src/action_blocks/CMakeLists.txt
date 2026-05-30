target_sources(pf2e_engine
    PRIVATE
        add_condition.cpp
        base_function.cpp
        block_input.cpp
        calculate_difficulty_class.cpp
        check_ally_adjacent.cpp
        choose_from_list.cpp
        contribute_damage_bonus.cpp
        deal_damage.cpp
        filter.cpp
        flat_damage_roll.cpp
        foldl.cpp
        get_parameter.cpp
        get_targets_in_area.cpp
        let.cpp
        map.cpp
        remove_condition.cpp
        roll_against_difficulty_class.cpp
        spell_damage_roll.cpp
        weapon_damage_roll.cpp
        move.cpp
)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/action_blocks
)
