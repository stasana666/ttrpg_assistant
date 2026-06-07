set(ARMOR_SCHEMA     ${CMAKE_SOURCE_DIR}/pf2e_engine/data/schemas/armor.ttrpg)
set(MATERIAL_SCHEMA  ${CMAKE_SOURCE_DIR}/pf2e_engine/data/schemas/material.ttrpg)
set(WEAPON_SCHEMA    ${CMAKE_SOURCE_DIR}/pf2e_engine/data/schemas/weapon.ttrpg)
set(DAMAGE_SCHEMA    ${CMAKE_SOURCE_DIR}/pf2e_engine/data/schemas/damage.ttrpg)
set(DICE_SCHEMA      ${CMAKE_SOURCE_DIR}/pf2e_engine/data/schemas/dice.ttrpg)
set(CREATURE_DATA_SCHEMA ${CMAKE_SOURCE_DIR}/pf2e_engine/data/schemas/creature_data.ttrpg)
set(CREATURE_PARTS_SCHEMA ${CMAKE_SOURCE_DIR}/pf2e_engine/data/schemas/creature_parts.ttrpg)
set(CONDITION_SCHEMA ${CMAKE_SOURCE_DIR}/pf2e_engine/data/schemas/condition.ttrpg)

set(ARMOR_GEN_H      ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/armor.h)
set(ARMOR_GEN_CPP    ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/armor.cpp)
set(MATERIAL_GEN_H   ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/material.h)
set(MATERIAL_GEN_CPP ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/material.cpp)
set(WEAPON_GEN_H     ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/weapon.h)
set(WEAPON_GEN_CPP   ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/weapon.cpp)
set(DAMAGE_GEN_H     ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/damage.h)
set(DAMAGE_GEN_CPP   ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/damage.cpp)
set(DICE_GEN_H       ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/dice.h)
set(DICE_GEN_CPP     ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/dice.cpp)
set(CREATURE_DATA_GEN_H   ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/creature_data.h)
set(CREATURE_DATA_GEN_CPP ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/creature_data.cpp)
set(CREATURE_PARTS_GEN_H   ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/creature_parts.h)
set(CREATURE_PARTS_GEN_CPP ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/creature_parts.cpp)
set(CONDITION_GEN_H   ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/condition.h)
set(CONDITION_GEN_CPP ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/condition.cpp)

add_custom_command(
    OUTPUT  ${MATERIAL_GEN_H} ${MATERIAL_GEN_CPP}
    COMMAND $<TARGET_FILE:ttrpg_codegen>
            --schema  ${MATERIAL_SCHEMA}
            --out-h   ${MATERIAL_GEN_H}
            --out-cpp ${MATERIAL_GEN_CPP}
    DEPENDS ttrpg_codegen ${MATERIAL_SCHEMA}
    VERBATIM
)

# armor.ttrpg imports material.ttrpg, so editing material.ttrpg must rebuild armor.
add_custom_command(
    OUTPUT  ${ARMOR_GEN_H} ${ARMOR_GEN_CPP}
    COMMAND $<TARGET_FILE:ttrpg_codegen>
            --schema  ${ARMOR_SCHEMA}
            --out-h   ${ARMOR_GEN_H}
            --out-cpp ${ARMOR_GEN_CPP}
    DEPENDS ttrpg_codegen ${ARMOR_SCHEMA} ${MATERIAL_SCHEMA}
    VERBATIM
)

add_custom_command(
    OUTPUT  ${DAMAGE_GEN_H} ${DAMAGE_GEN_CPP}
    COMMAND $<TARGET_FILE:ttrpg_codegen>
            --schema  ${DAMAGE_SCHEMA}
            --out-h   ${DAMAGE_GEN_H}
            --out-cpp ${DAMAGE_GEN_CPP}
    DEPENDS ttrpg_codegen ${DAMAGE_SCHEMA}
    VERBATIM
)

add_custom_command(
    OUTPUT  ${CONDITION_GEN_H} ${CONDITION_GEN_CPP}
    COMMAND $<TARGET_FILE:ttrpg_codegen>
            --schema  ${CONDITION_SCHEMA}
            --out-h   ${CONDITION_GEN_H}
            --out-cpp ${CONDITION_GEN_CPP}
    DEPENDS ttrpg_codegen ${CONDITION_SCHEMA}
    VERBATIM
)

add_custom_command(
    OUTPUT  ${DICE_GEN_H} ${DICE_GEN_CPP}
    COMMAND $<TARGET_FILE:ttrpg_codegen>
            --schema  ${DICE_SCHEMA}
            --out-h   ${DICE_GEN_H}
            --out-cpp ${DICE_GEN_CPP}
    DEPENDS ttrpg_codegen ${DICE_SCHEMA}
    VERBATIM
)

# weapon.ttrpg imports damage.ttrpg and dice.ttrpg, so editing either must rebuild weapon.
add_custom_command(
    OUTPUT  ${WEAPON_GEN_H} ${WEAPON_GEN_CPP}
    COMMAND $<TARGET_FILE:ttrpg_codegen>
            --schema  ${WEAPON_SCHEMA}
            --out-h   ${WEAPON_GEN_H}
            --out-cpp ${WEAPON_GEN_CPP}
    DEPENDS ttrpg_codegen ${WEAPON_SCHEMA} ${DAMAGE_SCHEMA} ${DICE_SCHEMA}
    VERBATIM
)

add_custom_command(
    OUTPUT  ${CREATURE_PARTS_GEN_H} ${CREATURE_PARTS_GEN_CPP}
    COMMAND $<TARGET_FILE:ttrpg_codegen>
            --schema  ${CREATURE_PARTS_SCHEMA}
            --out-h   ${CREATURE_PARTS_GEN_H}
            --out-cpp ${CREATURE_PARTS_GEN_CPP}
    DEPENDS ttrpg_codegen ${CREATURE_PARTS_SCHEMA}
    VERBATIM
)

# creature_data.ttrpg imports armor.ttrpg, weapon.ttrpg, creature_parts.ttrpg and condition.ttrpg
# (transitively material, damage, dice), so editing any of them must rebuild it.
add_custom_command(
    OUTPUT  ${CREATURE_DATA_GEN_H} ${CREATURE_DATA_GEN_CPP}
    COMMAND $<TARGET_FILE:ttrpg_codegen>
            --schema  ${CREATURE_DATA_SCHEMA}
            --out-h   ${CREATURE_DATA_GEN_H}
            --out-cpp ${CREATURE_DATA_GEN_CPP}
    DEPENDS ttrpg_codegen ${CREATURE_DATA_SCHEMA}
            ${ARMOR_SCHEMA} ${WEAPON_SCHEMA} ${MATERIAL_SCHEMA} ${DAMAGE_SCHEMA} ${DICE_SCHEMA}
            ${CREATURE_PARTS_SCHEMA} ${CONDITION_SCHEMA}
    VERBATIM
)

set_source_files_properties(
    ${ARMOR_GEN_CPP} ${MATERIAL_GEN_CPP} ${WEAPON_GEN_CPP} ${DAMAGE_GEN_CPP} ${DICE_GEN_CPP}
    ${CREATURE_DATA_GEN_CPP} ${CREATURE_PARTS_GEN_CPP} ${CONDITION_GEN_CPP}
    PROPERTIES GENERATED ON SKIP_LINTING ON
)
set_source_files_properties(
    ${ARMOR_GEN_H} ${MATERIAL_GEN_H} ${WEAPON_GEN_H} ${DAMAGE_GEN_H} ${DICE_GEN_H}
    ${CREATURE_DATA_GEN_H} ${CREATURE_PARTS_GEN_H} ${CONDITION_GEN_H}
    PROPERTIES GENERATED ON
)

add_custom_target(pf2e_engine_generated_headers
    DEPENDS ${ARMOR_GEN_H} ${ARMOR_GEN_CPP}
            ${MATERIAL_GEN_H} ${MATERIAL_GEN_CPP}
            ${WEAPON_GEN_H} ${WEAPON_GEN_CPP}
            ${DAMAGE_GEN_H} ${DAMAGE_GEN_CPP}
            ${DICE_GEN_H} ${DICE_GEN_CPP}
            ${CREATURE_DATA_GEN_H} ${CREATURE_DATA_GEN_CPP}
            ${CREATURE_PARTS_GEN_H} ${CREATURE_PARTS_GEN_CPP}
            ${CONDITION_GEN_H} ${CONDITION_GEN_CPP}
)
add_dependencies(pf2e_engine pf2e_engine_generated_headers)

target_sources(pf2e_engine
    PRIVATE
        ${ARMOR_GEN_CPP}
        ${MATERIAL_GEN_CPP}
        ${WEAPON_GEN_CPP}
        ${DAMAGE_GEN_CPP}
        ${DICE_GEN_CPP}
        ${CREATURE_DATA_GEN_CPP}
        ${CREATURE_PARTS_GEN_CPP}
        ${CONDITION_GEN_CPP}
)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/inventory
)
