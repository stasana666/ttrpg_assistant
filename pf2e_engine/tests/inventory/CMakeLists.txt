add_executable(test_inventory
    test_armor.cpp
)

target_link_libraries(test_inventory
    gtest_main
    pf2e_engine
)

target_include_directories(test_inventory
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/inventory
)

include(GoogleTest)
gtest_discover_tests(test_inventory)
