target_sources(pf2e_engine
    PRIVATE
        game_object_factory.cpp
        game_object_registry.cpp
)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/game_object_logic
)
