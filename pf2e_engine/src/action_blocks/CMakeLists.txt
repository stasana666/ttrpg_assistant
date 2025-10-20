target_sources(pf2e_engine
    PRIVATE
        attack_roll.cpp
        base_function.cpp
        block_input.cpp
        choose_target.cpp
        choose_weapon.cpp
)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/action_blocks
)
