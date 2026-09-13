add_executable(test_inventory
    test_armor.cpp
    test_armor_material_ref.cpp
    test_weapon_traits.cpp
    test_creature_data.cpp
    test_id_collection.cpp
    test_rule_entities.cpp
    test_granted_entities.cpp
)

set(RULE_ENTITIES_HEADER ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/rule_entities.h)
set(RULE_ENTITIES_SOURCE ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/rule_entities.cpp)
ttrpg_generate(
    TARGET test_inventory
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/rule_entities.ttrpg
    HEADER ${RULE_ENTITIES_HEADER}
    SOURCE ${RULE_ENTITIES_SOURCE}
)
target_sources(test_inventory PRIVATE ${RULE_ENTITIES_HEADER} ${RULE_ENTITIES_SOURCE})

foreach(GRANTED_FIXTURE IN ITEMS granted_abilities granted_entities)
    set(GRANTED_HEADER ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/${GRANTED_FIXTURE}.h)
    set(GRANTED_SOURCE ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/${GRANTED_FIXTURE}.cpp)
    ttrpg_generate(
        TARGET test_inventory
        SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${GRANTED_FIXTURE}.ttrpg
        HEADER ${GRANTED_HEADER}
        SOURCE ${GRANTED_SOURCE}
    )
    target_sources(test_inventory PRIVATE ${GRANTED_HEADER} ${GRANTED_SOURCE})
endforeach()

target_link_libraries(test_inventory
    gtest_main
    pf2e_engine
)

target_include_directories(test_inventory
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/inventory
    PRIVATE ${CMAKE_BINARY_DIR}/cpp_config
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/tests/test_lib
)

include(GoogleTest)
gtest_discover_tests(test_inventory)
