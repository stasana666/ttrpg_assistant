add_library(pf2e_engine)

target_include_directories(pf2e_engine
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_subdirectory(tests)
add_subdirectory(src)
