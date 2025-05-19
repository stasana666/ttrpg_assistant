add_library(pf2e_engine
    src/characteristics.cpp
    src/creature.cpp
    src/armour_class.cpp
)

target_include_directories(pf2e_engine
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/pf2e_engine
)

add_subdirectory(tests)
add_subdirectory(src/inventory)
add_subdirectory(src/mechanics)
add_subdirectory(src/dice_expression)
