# `.ttrpg` Language Tooling

## Purpose
- Parser, module loader, semantic analysis, C++ writer/emitter, tests, and `ttrpg_codegen` CLI for the `.ttrpg` schema language.
- This is separate from the engine runtime DSL in `pf2e_engine/src/dsl`.

## Key Files
- `include/ttrpg/schema_ast.h`: AST for enums, classes, variants, fields, containers, and derives.
- `src/parser.cpp`: tokenization and grammar.
- `src/module.cpp`: imports, duplicate symbol checks, circular import detection.
- `src/analyze.cpp`: computed default/derive validation and field init ordering.
- `src/conventions.cpp`: type mapping, JSON key naming, default lowering.
- `src/emit.cpp`: generated C++ headers/implementations.
- `src/cpp_writer.cpp`: indentation/braces for generated output; there is no external formatting pass.
- `tests/fixtures/*.golden.*`: golden output for generated C++.

## Build/Test
- Build CLI: `cmake --build build --target ttrpg_codegen`.
- Run tests: `cmake --build build --target test_ttrpg` then `ctest --test-dir build --output-on-failure`.
- Update golden fixtures only when generated API/output is intentionally changed.

## Pitfalls
- Keep generated output deterministic; golden tests compare exact strings.
- Initializer text after `=` is parsed by shared `expr`; codegen accepts only its supported subset and rejects calls, `$` vars, comparisons, and logical operators.
- `derive` fields have no storage; bare references to derived fields are rejected by analysis.
- Variant alternatives currently reject computed initializers and derived fields.
- If grammar changes, update parser tests, analyzer tests, and golden fixtures together.
