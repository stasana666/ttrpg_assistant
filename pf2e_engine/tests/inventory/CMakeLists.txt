add_executable(test_inventory
    test_armour.cpp
)

target_link_libraries(test_inventory
    gtest_main
    inventory
)

include(GoogleTest)
gtest_discover_tests(test_inventory)
