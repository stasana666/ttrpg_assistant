add_library(pf2e_engine)

target_include_directories(pf2e_engine
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
           ${CMAKE_BINARY_DIR}/generated/pf2e_engine/include
)

add_subdirectory(src)
add_subdirectory(tests)
