# TTRPG Assistant

Canonical project instructions. CLAUDE.md redirects here.

## Purpose
- C++23 CMake project for a PF2E/TTRPG combat assistant.
- Main first-party areas:
  - `pf2e_engine/`: headless game-mechanics engine.
  - `assistant/`: player-facing GUI/CLI/voice app.
  - `analyzer/`: headless Monte-Carlo combat simulator.
  - `tools/`: parser/codegen/data-validation tooling.
- Prefer local `AGENTS.md` files before loading broader context. `.ttrpg` tooling, assistant/voice, analyzer simulation, and engine internals should stay mentally separate unless a task crosses boundaries.

## Documentation Policy
- Do not add comments to source code or `.ttrpg` schemas.
- Allowed exceptions: existing/actionable TODOs and generated-file banners emitted by codegen.
- Put project knowledge in the nearest `AGENTS.md`, not in code comments.
- `CLAUDE.md` files contain only a one-line link to the canonical `AGENTS.md` in the same directory.

## Commits
- Commit completed changes after the relevant build, tests, and validation pass. For work spanning several stages, commit each completed stage separately before continuing.
- Do not ask for confirmation to create these local commits. Keep unrelated pre-existing changes separate from the task's commits.
- Use commit messages that describe the resulting behavior. Do not push commits unless the user requests it.

## Build/Test
- Configure: `cmake -S . -B build -DGGML_CUDA=ON`.
- Build all: `cmake --build build`.
- Run registered tests: `ctest --test-dir build --output-on-failure`.
- Common targets: `pf2e_engine`, `assistant`, `analyzer`, `ttrpg_codegen`, `test_expr`, `test_ttrpg`, `test_inventory`, `test_dsl`.
- `test_actions` is registered with CTest through `gtest_discover_tests`; it can also run directly as `./build/pf2e_engine/tests/actions/test_actions`.
- Validate data: `python3 tools/data_validation.py -s pf2e_engine/schemas/schema.json -t pf2e_engine/data`.

## Build System
- Top-level CMake enables C++23, testing, warnings as errors, and global `clang-tidy`.
- CMake 3.27 or newer is required for `SKIP_LINTING` on generated sources; generation helpers apply source properties in the consuming target's directory.
- Add order is `extern`, shared parser/expression tools, `.ttrpg` tooling, then `pf2e_engine`, `assistant`, and `analyzer`.
- Generated config `cpp_config.h` provides `kRootDirPath`; data-loading tests and apps use it to find `pf2e_engine/data`.
- `tools/ttrpg` and `tools/common/expr` opt out of clang-tidy in their CMake because they use parser/codegen naming and generated-string patterns.

## Conventions
- Formatting follows `.clang-format` (Google-based, pointer left, sorted includes disabled).
- clang-tidy naming expectations: CamelCase types/functions, `k` constants, lower-case locals/parameters, private members ending in `_`.
- Type prefixes are common and intentional: `T` types, `E` enums, `I` interfaces, `F` action-block callables.
- Do not edit `extern/`, `build/`, `build/generated/`, `venv/`, or `models/` unless explicitly requested.
- Do not edit generated C++ under `build/generated`; edit `.ttrpg` schemas or codegen instead.

## Local Docs
- Engine overview and action-extension rules: `pf2e_engine/AGENTS.md`.
- Engine JSON data: `pf2e_engine/data/AGENTS.md`.
- Engine `.ttrpg` schemas: `pf2e_engine/data/schemas/AGENTS.md`.
- Runtime DSL evaluator: `pf2e_engine/src/dsl/AGENTS.md`.
- Continuations and variant utilities: `pf2e_engine/include/pf2e_engine/common/AGENTS.md`.
- AST rollback comparison: `pf2e_engine/include/pf2e_engine/common/ast/AGENTS.md`.
- Engine tests and mocks: `pf2e_engine/tests/AGENTS.md`.
- `.ttrpg` parser/codegen: `tools/ttrpg/AGENTS.md`.
- Shared parser/expression layers: `tools/common/AGENTS.md`, `tools/common/parse/AGENTS.md`, `tools/common/expr/AGENTS.md`.
- Assistant app: `assistant/AGENTS.md`.
- Analyzer simulator: `analyzer/AGENTS.md`.
- Vendored dependencies: `extern/AGENTS.md`.

## Cross-Cutting Facts
- `pf2e_engine` links `nlohmann_json`, `nlohmann_json_schema_validator`, `ttrpg_parse`, and `expr`.
- The engine talks to outside systems through `IInteractionSystem`; assistant, analyzer, and tests provide implementations.
- The runtime DSL and `.ttrpg` codegen share `tools/common/expr` but use different semantics: the DSL evaluates the full grammar, while codegen lowers only supported initializer expressions.
- `.ttrpg` schemas generate C++ into `build/generated/pf2e_engine/...`; CMake target `pf2e_engine_generated_headers` controls generation ordering.
- Generated schema classes do not need handwritten AST layout asserts; handwritten AST-instrumented engine classes do.

## Verification
- Use the smallest relevant target first, then broader `ctest` when shared behavior changed.
- Schema/codegen changes: build `test_ttrpg` and `pf2e_engine_generated_headers`, then relevant engine/data tests.
- JSON data changes: run `tools/data_validation.py` and tests that load data recursively.
- Assistant has no dedicated test target; verify engine-facing behavior via engine tests and manually test GUI/audio changes when needed.
- Analyzer has no dedicated test target; verify combat behavior through relevant engine tests.
