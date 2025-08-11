target_sources(pf2e_engine
    PRIVATE
        game_object_factory.cpp
        game_object_storage.cpp
)

add_subdirectory(${CMAKE_SOURCE_DIR}/extern/json ${CMAKE_BINARY_DIR}/extern/json)

target_link_libraries(pf2e_engine PRIVATE nlohmann_json::nlohmann_json)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/game_object_logic
)
