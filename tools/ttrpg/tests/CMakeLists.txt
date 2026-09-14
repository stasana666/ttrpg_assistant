add_executable(test_ttrpg
    test_writer.cpp
    test_conventions.cpp
    test_parser.cpp
    test_analyze.cpp
    test_abilities.cpp
    test_activities.cpp
    test_actions.cpp
    test_action_flow.cpp
    test_events.cpp
    test_sources.cpp
    test_source_choices.cpp
    test_world.cpp
    test_operations.cpp
    test_golden.cpp
)

target_link_libraries(test_ttrpg
    gtest_main
    ttrpg
)

# test_parser / test_golden read .ttrpg fixtures + golden output from this dir.
target_compile_definitions(test_ttrpg PRIVATE
    TTRPG_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

# The language tooling opts out of clang-tidy; its tests do too.
set_target_properties(test_ttrpg PROPERTIES CXX_CLANG_TIDY "")

include(GoogleTest)
gtest_discover_tests(test_ttrpg)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_dependencies(test_ttrpg ttrpg_codegen)
add_test(NAME TtrpgCliTest
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_cli.py"
        --codegen $<TARGET_FILE:ttrpg_codegen>
        --cmake "${CMAKE_COMMAND}"
        --helper "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/TtrpgCodegen.cmake"
)
