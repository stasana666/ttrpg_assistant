include(GoogleTest)

function(add_engine_test name)
    cmake_parse_arguments(PARSE_ARGV 1 TEST "" "" "SOURCES;SCHEMAS;INCLUDES")
    add_executable(${name} ${TEST_SOURCES})
    foreach(SCHEMA IN LISTS TEST_SCHEMAS)
        get_filename_component(STEM ${SCHEMA} NAME_WE)
        set(HEADER ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include/pf2e_engine/inventory/${STEM}.h)
        set(SOURCE ${CMAKE_BINARY_DIR}/generated/pf2e_engine/src/inventory/${STEM}.cpp)
        ttrpg_generate(TARGET ${name} SCHEMA ${SCHEMA} HEADER ${HEADER} SOURCE ${SOURCE})
        target_sources(${name} PRIVATE ${HEADER} ${SOURCE})
    endforeach()
    target_link_libraries(${name} gtest_main pf2e_engine)
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/pf2e_engine/tests/test_lib ${TEST_INCLUDES})
    gtest_discover_tests(${name})
endfunction()

add_engine_test(test_pf2_engine
    SOURCES test_alternatives.cpp test_armor.cpp test_resources.cpp test_random.cpp test_continuation.cpp
        test_battle_map.cpp)
add_engine_test(test_lifecycle SOURCES test_lifecycle.cpp)
add_engine_test(test_lifecycle_execution
    SOURCES test_lifecycle_execution.cpp
    SCHEMAS actions/fixtures/generated_lifecycle.ttrpg)
add_engine_test(test_initiative SOURCES test_initiative.cpp)
add_engine_test(test_knockout_initiative SOURCES test_knockout_initiative.cpp)
add_engine_test(test_health SOURCES test_health.cpp)

add_subdirectory(actions)
add_subdirectory(ast)
add_subdirectory(inventory)
add_subdirectory(mechanics)
add_subdirectory(transformation)
