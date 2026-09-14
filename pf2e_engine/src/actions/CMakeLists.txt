target_sources(pf2e_engine
    PRIVATE
        action_reader.cpp
        action_block.cpp
        action.cpp
        ability.cpp
        generated_action.cpp
        generated_source.cpp
        generated_world.cpp
        feature_event.cpp
        reaction.cpp
        save_point.cpp
)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/actions
)
