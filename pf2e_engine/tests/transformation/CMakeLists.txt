add_executable(test_transformation
    test_transformation.cpp
)

target_link_libraries(test_transformation
    gtest_main
    pf2e_engine
)

target_include_directories(test_transformation
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/transformation
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/tests/test_lib
    PRIVATE ${CMAKE_BINARY_DIR}/cpp_config
)

include(GoogleTest)
gtest_discover_tests(test_transformation)
