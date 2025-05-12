add_library(pf2e_engine
    src/hitpoints.cpp
)

target_include_directories(pf2e_engine
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/pf2e_engine
)

add_subdirectory(tests)
