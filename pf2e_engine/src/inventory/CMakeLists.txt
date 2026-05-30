set(ARMOR_SCHEMA  ${CMAKE_SOURCE_DIR}/pf2e_engine/data/schemas/armor.ttrpg)
set(ARMOR_GEN_H   ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/armor.h)
set(ARMOR_GEN_CPP ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/armor.cpp)

add_custom_command(
    OUTPUT  ${ARMOR_GEN_H} ${ARMOR_GEN_CPP}
    COMMAND $<TARGET_FILE:ttrpg_codegen>
            --schema  ${ARMOR_SCHEMA}
            --out-h   ${ARMOR_GEN_H}
            --out-cpp ${ARMOR_GEN_CPP}
    DEPENDS ttrpg_codegen ${ARMOR_SCHEMA}
    VERBATIM
)

set_source_files_properties(${ARMOR_GEN_CPP} PROPERTIES
    GENERATED ON
    SKIP_LINTING ON
)
set_source_files_properties(${ARMOR_GEN_H} PROPERTIES
    GENERATED ON
)

add_custom_target(pf2e_engine_generated_headers
    DEPENDS ${ARMOR_GEN_H} ${ARMOR_GEN_CPP}
)
add_dependencies(pf2e_engine pf2e_engine_generated_headers)

target_sources(pf2e_engine
    PRIVATE
        weapon.cpp
        ${ARMOR_GEN_CPP}
)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/inventory
)
