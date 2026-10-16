target_sources(pf2e_engine
    PRIVATE
        action_reader.cpp
        action_block.cpp
        action.cpp
        resource_payment.cpp
        reaction_window.cpp
        lifecycle_execution.cpp
        ability.cpp
        generated_action.cpp
        generated_source.cpp
        generated_world.cpp
        generated_movement.cpp
        generated_initiative.cpp
        action_history.cpp
        generated_history.cpp
        timed_relation.cpp
        generated_relation.cpp
        feature_event.cpp
        save_point.cpp
)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/actions
)
