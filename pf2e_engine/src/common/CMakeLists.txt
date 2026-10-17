target_sources(pf2e_engine
    PRIVATE
        bounded_quantity.cpp
        resource.cpp
        entity_id.cpp
        generated_execution.cpp
        game_time.cpp
        ability.cpp
)

add_subdirectory(ast)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/common
)
