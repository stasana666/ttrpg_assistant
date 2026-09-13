set(HITPOINTS_FIXTURE_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/include/generated_hitpoints_fixture.h")
set(HITPOINTS_FIXTURE_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/src/generated_hitpoints_fixture.cpp")
add_executable(test_transformation
    test_transformation.cpp
    test_generated_execution.cpp
    test_hitpoints_execution.cpp
    "${HITPOINTS_FIXTURE_SOURCE}"
)
ttrpg_generate(
    TARGET test_transformation
    SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/generated_hitpoints_fixture.ttrpg"
    HEADER "${HITPOINTS_FIXTURE_HEADER}"
    SOURCE "${HITPOINTS_FIXTURE_SOURCE}"
)
add_custom_target(test_transformation_generated_headers
    DEPENDS "${HITPOINTS_FIXTURE_HEADER}" "${HITPOINTS_FIXTURE_SOURCE}"
)

add_dependencies(test_transformation test_transformation_generated_headers)

target_link_libraries(test_transformation
    gtest_main
    pf2e_engine
)

target_include_directories(test_transformation
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/transformation
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/tests/test_lib
    PRIVATE ${CMAKE_BINARY_DIR}/cpp_config
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/include"
    PRIVATE "${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory"
)

include(GoogleTest)
gtest_discover_tests(test_transformation)
