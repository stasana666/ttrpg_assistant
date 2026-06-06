# TTRPG Assistant

## Purpose
- C++23 CMake project for a PF2E/TTRPG engine, interactive assistant, combat analyzer, and first-party tooling.
- Keep subsystem context local: engine work belongs under `pf2e_engine/`, `.ttrpg` language work under `tools/ttrpg/` or `pf2e_engine/data/schemas/`, assistant work under `assistant/`, and simulation work under `analyzer/`.
- `extern/`, `venv/`, `models/`, `build/`, and `build/generated/` are dependencies, local environment, or generated output.

## Build System
- Configure: `cmake -S . -B build -DGGML_CUDA=ON` (CUDA is optional, but voice inference is slow on CPU).
- Build all: `cmake --build build`.
- Common targets: `pf2e_engine`, `assistant`, `analyzer`, `ttrpg_codegen`, `test_expr`, `test_ttrpg`, `test_inventory`, `test_dsl`.
- Run tests: `ctest --test-dir build --output-on-failure`.
- Validate data JSON: `python3 tools/data_validation.py -s pf2e_engine/schemas/schema.json -t pf2e_engine/data`.

## Conventions
- Formatting follows `.clang-format` (Google-based, pointer left, sorted includes disabled).
- Root CMake enables `-Wall -Wextra -Wpedantic -Werror -g` and global `clang-tidy`; language-tooling targets opt out in their CMake files.
- Naming is clang-tidy enforced: CamelCase types/functions, `k` constants, lower-case locals/parameters, private members ending in `_`.
- Tests use GoogleTest and are registered mostly with `gtest_discover_tests`.
- Do not add comments to source files or `.ttrpg` schemas. Put project knowledge in the nearest `AGENTS.md`; keep only existing TODOs and generated-file banners.

## Verification
- Prefer the smallest relevant build/test target first, then broader `ctest` when shared behavior changed.
- For `.ttrpg` schema/codegen changes, build `test_ttrpg` and `pf2e_engine_generated_headers`; then run relevant engine inventory/data tests.
- For JSON data changes, run the validation script and the engine tests that load data recursively.

## Do Not
- Do not edit `extern/`, `build/`, `build/generated/`, `venv/`, or `models/` unless explicitly requested.
- Do not edit generated C++ under `build/generated`; edit `.ttrpg` schemas or codegen instead.
- Do not mix unrelated context, such as loading assistant/audio details for `.ttrpg` parser work.
- Do not commit, reset, or otherwise use git operations unless explicitly requested.
