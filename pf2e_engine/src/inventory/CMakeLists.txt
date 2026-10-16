set(TTRPG_ENGINE_SCHEMA_DIR "${CMAKE_SOURCE_DIR}/pf2e_engine/data/schemas")
set(TTRPG_ENGINE_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/pf2e_engine")
set(TTRPG_ENGINE_GENERATED_OUTPUTS)

foreach(SCHEMA_NAME IN ITEMS material armor shield shield_actions intimidation trip remaster_profiles damage dice statistic_baseline combat_checks combat_geometry combat_timing abilities flanking movement weapon skills saving_throws proficiency creature_parts creature_description check_modifiers statistics lore hitpoints health creature)
    set(GENERATED_HEADER "${TTRPG_ENGINE_GENERATED_DIR}/include/pf2e_engine/inventory/${SCHEMA_NAME}.h")
    set(GENERATED_SOURCE "${TTRPG_ENGINE_GENERATED_DIR}/src/inventory/${SCHEMA_NAME}.cpp")
    ttrpg_generate(
        TARGET pf2e_engine
        SCHEMA "${TTRPG_ENGINE_SCHEMA_DIR}/${SCHEMA_NAME}.ttrpg"
        HEADER "${GENERATED_HEADER}"
        SOURCE "${GENERATED_SOURCE}"
    )
    list(APPEND TTRPG_ENGINE_GENERATED_OUTPUTS "${GENERATED_HEADER}" "${GENERATED_SOURCE}")
    target_sources(pf2e_engine PRIVATE "${GENERATED_SOURCE}")
endforeach()

set(CONDITION_GEN_H "${TTRPG_ENGINE_GENERATED_DIR}/include/pf2e_engine/condition.h")
set(CONDITION_GEN_CPP "${TTRPG_ENGINE_GENERATED_DIR}/src/condition.cpp")
ttrpg_generate(
    TARGET pf2e_engine
    SCHEMA "${TTRPG_ENGINE_SCHEMA_DIR}/condition.ttrpg"
    HEADER "${CONDITION_GEN_H}"
    SOURCE "${CONDITION_GEN_CPP}"
)
list(APPEND TTRPG_ENGINE_GENERATED_OUTPUTS "${CONDITION_GEN_H}" "${CONDITION_GEN_CPP}")
target_sources(pf2e_engine PRIVATE "${CONDITION_GEN_CPP}")

add_custom_target(pf2e_engine_generated_headers DEPENDS ${TTRPG_ENGINE_GENERATED_OUTPUTS})
add_dependencies(pf2e_engine pf2e_engine_generated_headers)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/inventory
)
