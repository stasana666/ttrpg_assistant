target_sources(pf2e_engine
    PRIVATE
        hash_combine.cpp
)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/common
)
