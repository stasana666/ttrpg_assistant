add_subdirectory(${CMAKE_SOURCE_DIR}/extern/json ${CMAKE_BINARY_DIR}/extern/json)
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/json-schema-validator ${CMAKE_BINARY_DIR}/extern/json-schema-validator)
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/googletest ${CMAKE_BINARY_DIR}/extern/googletest)

add_subdirectory(${CMAKE_SOURCE_DIR}/extern/sfml ${CMAKE_BINARY_DIR}/extern/sfml)

add_library(vosk::vosk SHARED IMPORTED GLOBAL)
if(APPLE)
    set(_VOSK_LIB "${CMAKE_SOURCE_DIR}/extern/vosk/lib/osx/libvosk.dylib")
    set_target_properties(vosk::vosk PROPERTIES IMPORTED_LOCATION "${_VOSK_LIB}")
elseif(WIN32)
    set(_VOSK_DLL "${CMAKE_SOURCE_DIR}/extern/vosk/lib/win64/vosk.dll")
    set(_VOSK_LIB "${CMAKE_SOURCE_DIR}/extern/vosk/lib/win64/vosk.lib")
    set_target_properties(vosk::vosk PROPERTIES
        IMPORTED_LOCATION "${_VOSK_DLL}"
        IMPORTED_IMPLIB  "${_VOSK_LIB}"
    )
else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        set(_VOSK_LIB "${CMAKE_SOURCE_DIR}/extern/vosk/lib/linux/x86_64/libvosk.so")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(_VOSK_LIB "${CMAKE_SOURCE_DIR}/extern/vosk/lib/linux/aarch64/libvosk.so")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7l|armv7")
        set(_VOSK_LIB "${CMAKE_SOURCE_DIR}/extern/vosk/lib/linux/armv7l/libvosk.so")
    else()
        message(FATAL_ERROR "Unsupported arch for Vosk: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    set_target_properties(vosk::vosk PROPERTIES IMPORTED_LOCATION "${_VOSK_LIB}")
endif()

set_target_properties(vosk::vosk PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/extern/vosk-api/src"
)
