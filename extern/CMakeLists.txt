add_subdirectory(${CMAKE_SOURCE_DIR}/extern/json ${CMAKE_BINARY_DIR}/extern/json)
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/json-schema-validator ${CMAKE_BINARY_DIR}/extern/json-schema-validator)
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/googletest ${CMAKE_BINARY_DIR}/extern/googletest)
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/sfml ${CMAKE_BINARY_DIR}/extern/sfml)
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/llama ${CMAKE_BINARY_DIR}/extern/llama)
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/argparse ${CMAKE_BINARY_DIR}/extern/argparse)

set(VOSK_LIB ${CMAKE_SOURCE_DIR}/venv/lib64/python3.12/site-packages/vosk/libvosk.so)
set(VOSK_INCLUDE ${CMAKE_SOURCE_DIR}/extern/vosk-api/src)

add_library(vosk SHARED IMPORTED GLOBAL)
set_target_properties(vosk PROPERTIES
    IMPORTED_LOCATION ${VOSK_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${VOSK_INCLUDE}
)
