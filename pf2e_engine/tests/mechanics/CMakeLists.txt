add_engine_test(test_mechanics
    SOURCES
        test_bounded_quantity.cpp
        test_characteristics.cpp
        test_damage_resolver.cpp
        test_proficiency_calculation.cpp
        test_generated_combat_rules.cpp
        test_generated_statistics.cpp
    INCLUDES ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/mechanics)
