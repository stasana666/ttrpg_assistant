add_library(inventory 
    armour.cpp
)

target_include_directories(inventory
    PUBLIC ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/inventory
)
