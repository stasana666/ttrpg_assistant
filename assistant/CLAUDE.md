# assistant — player-facing application (GUI / CLI / voice)

Canonical documentation file. AGENTS.md must remain semantically equivalent.

The human-facing app built on top of `pf2e_engine` (`assistant_lib` library +
`assistant` executable; links SFML, llama.cpp, Vosk, argparse on top of the
engine). It supplies the engine's only outward dependency, the
`IInteractionSystem` implementation — see the root [CLAUDE.md](../CLAUDE.md)
"Interaction System" section for the engine-side interface contract. **Engine
internals are not needed for most assistant work**; treat the engine through
`IInteractionSystem` / `TBattle`.

## Layout

- [main/main.cpp](main/main.cpp) — arg parsing, game-object loading, battle
  setup, and the GUI/game-thread split.
- [src/interaction_system.cpp](src/interaction_system.cpp) — `TInteractionSystem`,
  the concrete `IInteractionSystem`. Multiple simultaneous input sources (GUI,
  CLI, voice) over channel-based communication (`TChannel<TClickEvent>`); the GUI
  runs on the main thread, game logic on a separate thread. **Asking strategies**:
  `EAskingStrategy::Console` (CLI/voice) and `EAskingStrategy::Gui` (click input).
  It defers reaction triggers (throws `TSavepointStackUnwind`) so the game flow is
  not blocked — the opposite of the analyzer, which resolves them immediately.
- [src/gui/](src/gui/) — SFML board rendering and texture storage; assets live in
  [images/](images/).
- [src/audio_input/](src/audio_input/) — `TAudioInputSystem`: Vosk speech-to-text
  + llama.cpp intent recognition, plus the prompt/audio worker.

## Build / run

- Build: `cmake --build build --target assistant`.
- Run (GUI + CLI, no voice): `./build/assistant/assistant`.
- With voice: `./build/assistant/assistant --speech2text <vosk-model> --nlp-model <llama.gguf>`.
  Both flags are required to enable audio/NLP; otherwise audio is disabled.
  Models — Vosk: https://alphacephei.com/vosk/models ; llama.cpp:
  https://huggingface.co/models?apps=llama.cpp
- No dedicated assistant test target; verify engine-facing logic via engine tests.

## Vosk configuration

The path to `libvosk.so` is set via the `VOSK_LIB` variable in
[../extern/CMakeLists.txt](../extern/CMakeLists.txt) (default assumes a Python
venv install). Update it if Vosk is installed elsewhere. Models are runtime
inputs, not source.

## Pitfalls

- Threads/queues in `TInteractionSystem` and `TAudioInputSystem` are subtle;
  manual testing may be needed for changes there.
- Avoid loading or modifying assistant code for schema/codegen-only or analyzer-only tasks.
- Do not edit vendored llama / Vosk / SFML code under `extern/` for assistant work.
- Spinlock polling is a known limitation (TODO: condition_variable).
