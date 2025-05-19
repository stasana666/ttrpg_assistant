add_executable(test_dice_expression
    test_dice.cpp
    test_random.cpp
)

target_link_libraries(test_dice_expression
    gtest_main
    pf2e_engine
)

target_include_directories(test_dice_expression
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/dice_expression
)

include(GoogleTest)
gtest_discover_tests(test_dice_expression)
