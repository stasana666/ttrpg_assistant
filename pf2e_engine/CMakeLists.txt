add_library(pf2e_engine)

target_include_directories(pf2e_engine
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_BINARY_DIR}/cpp_config
)

target_link_libraries(pf2e_engine PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(pf2e_engine PUBLIC nlohmann_json_schema_validator)
target_link_libraries(pf2e_engine PUBLIC sfml-audio)
target_link_libraries(pf2e_engine PUBLIC sfml-graphics)
target_link_libraries(pf2e_engine PUBLIC sfml-system)
target_link_libraries(pf2e_engine PUBLIC sfml-window)
target_link_libraries(pf2e_engine PUBLIC llama)
#target_link_libraries(pf2e_engine PUBLIC vosk::vosk)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(main)
