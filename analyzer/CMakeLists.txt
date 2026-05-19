add_library(analyzer_lib)

target_include_directories(analyzer_lib
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_BINARY_DIR}/cpp_config
)

target_link_libraries(analyzer_lib PUBLIC pf2e_engine)

add_subdirectory(src)

add_executable(analyzer main/analyzer_main.cpp)

target_link_libraries(analyzer analyzer_lib)
