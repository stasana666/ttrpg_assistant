# Vendored Dependencies

## Purpose
- Third-party dependencies pulled into the build: nlohmann JSON, JSON Schema validator, GoogleTest, SFML, llama.cpp, argparse, and imported Vosk headers/library.

## Guidance
- Treat this directory as read-only unless the user explicitly asks to patch vendored code.
- Prefer changing first-party CMake or wrapper code over editing vendored sources.
- `extern/CMakeLists.txt` wires dependencies into the root build and imports Vosk from the local Python environment.

## Pitfalls
- Do not run broad formatting or refactors here.
- Do not add generated/build artifacts here.
- If a build issue points into `extern/`, first confirm whether it is configuration, missing submodules, or missing local Vosk/model files.
