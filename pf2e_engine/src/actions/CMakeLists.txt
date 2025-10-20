target_sources(pf2e_engine
    PRIVATE
        action_reader.cpp
        action_block.cpp
        action.cpp
)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/actions
)
