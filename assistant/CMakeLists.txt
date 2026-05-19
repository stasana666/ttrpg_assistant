add_library(assistant_lib)

target_include_directories(assistant_lib
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_BINARY_DIR}/cpp_config
)

target_link_libraries(assistant_lib PUBLIC pf2e_engine)
target_link_libraries(assistant_lib PUBLIC sfml-audio)
target_link_libraries(assistant_lib PUBLIC sfml-graphics)
target_link_libraries(assistant_lib PUBLIC sfml-system)
target_link_libraries(assistant_lib PUBLIC sfml-window)
target_link_libraries(assistant_lib PUBLIC llama)
target_link_libraries(assistant_lib PUBLIC vosk)

add_subdirectory(src)

add_executable(assistant main/main.cpp)

target_include_directories(assistant
    PRIVATE ${CMAKE_BINARY_DIR}/cpp_config
)

target_link_libraries(assistant assistant_lib argparse)
