add_executable(main
    main.cpp
)

target_include_directories(main
    PRIVATE ${CMAKE_BINARY_DIR}/cpp_config
)

target_link_libraries(main
    pf2e_engine
    nlohmann_json::nlohmann_json
)
