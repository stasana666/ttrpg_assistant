add_engine_test(test_transformation
    SOURCES test_transformation.cpp test_generated_execution.cpp test_hitpoints_execution.cpp
    SCHEMAS generated_hitpoints_fixture.ttrpg
    INCLUDES
        ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/transformation
        ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory)
