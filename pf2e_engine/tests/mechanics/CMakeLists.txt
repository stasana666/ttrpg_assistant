add_executable(test_mechanics
    test_hitpoints.cpp
    test_characteristics.cpp
)

target_link_libraries(test_mechanics
    gtest_main
    pf2e_engine
)

target_include_directories(test_mechanics
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/mechanics
)

include(GoogleTest)
gtest_discover_tests(test_mechanics)
