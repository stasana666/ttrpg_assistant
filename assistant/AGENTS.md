# Assistant

## Purpose
- Interactive application on top of `pf2e_engine`.
- Combines console choices, optional voice/NLP input, and an SFML board GUI.

## Key Files
- `main/main.cpp`: argument parsing, game object loading, battle setup, GUI/game threads.
- `src/interaction_system.cpp`: implements `IInteractionSystem` with console, click, and optional audio input.
- `src/gui/`: SFML board rendering and texture storage.
- `src/audio_input/`: Vosk speech-to-text, llama.cpp intent recognition, prompt/audio worker.
- `images/`: GUI assets referenced by player image paths.

## Build/Test
- Build: `cmake --build build --target assistant`.
- Run manually from the build output; `--speech2text` and `--nlp-model` enable audio/NLP. Without both, audio is disabled.
- No dedicated assistant test target exists; verify engine-facing logic with engine tests where possible.

## Pitfalls
- Vosk library path is configured in `extern/CMakeLists.txt`; models are runtime inputs, not source files.
- Avoid loading or modifying assistant code for schema/codegen-only or analyzer-only tasks.
- Be careful with threads and queues in `TInteractionSystem` and `TAudioInputSystem`; manual testing may be needed.
- Do not edit vendored llama/vosk/SFML code for assistant changes.
