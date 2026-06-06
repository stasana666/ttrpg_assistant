# Tools

## Purpose
- First-party tooling shared by the engine and schema pipeline.
- `common/parse`: scanner and token stream helpers.
- `common/expr`: backend-agnostic expression lexer/parser/AST.
- `ttrpg`: `.ttrpg` schema language parser, analyzer, C++ emitter, and CLI.
- `data_validation.py`: Python JSON Schema validator for `pf2e_engine/data`.

## Build/Test
- Build language tools: `cmake --build build --target ttrpg_codegen`.
- Test expression parser: `cmake --build build --target test_expr`.
- Test `.ttrpg` tooling: `cmake --build build --target test_ttrpg`.
- Validate data from repo root: `python3 tools/data_validation.py -s pf2e_engine/schemas/schema.json -t pf2e_engine/data`.

## Conventions
- `expr` and `ttrpg` targets opt out of clang-tidy in CMake; still keep code readable and covered by tests.
- Keep `common` libraries backend-agnostic: no PF2E, assistant, or analyzer dependencies.
- Use parser/scanner helpers instead of ad hoc string parsing.

## Pitfalls
- The `.ttrpg` code generator and engine runtime DSL both use `expr`, but they have different allowed semantics.
- Do not update engine generated output manually; update schemas or `tools/ttrpg` behavior and rerun generation.
