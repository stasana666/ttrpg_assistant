add_subdirectory(${CMAKE_SOURCE_DIR}/extern/googletest ${CMAKE_BINARY_DIR}/extern/googletest)

add_executable(test_pf2_engine
    test_armor.cpp
    test_resources.cpp
    test_weapon_damage.cpp
)

target_link_libraries(test_pf2_engine
    gtest_main
    pf2e_engine
)

target_include_directories(test_pf2_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/tests/test_lib
)

include(GoogleTest)
gtest_discover_tests(test_pf2_engine)

add_subdirectory(inventory)
add_subdirectory(mechanics)
add_subdirectory(dice_expression)
