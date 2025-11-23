target_sources(pf2e_engine
    PRIVATE
        audio_input_system.cpp
        audio_input.cpp
        intent_recognizer.cpp
        prompt.cpp
        speech_to_text.cpp
)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/audio_input
)
