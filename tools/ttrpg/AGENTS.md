# `ttrpg` Schema Language Tooling

Canonical project instructions. CLAUDE.md redirects here.

## Purpose
- C++23 static library `ttrpg` plus thin CLI `ttrpg_codegen`.
- Turns one `.ttrpg` schema module into generated `*.h` / `*.cpp`.
- The schema is the source of truth for generated C++ type shape, JSON loading, AST serialization, and DSL property registration.

## Build/Test
- Build CLI: `cmake --build build --target ttrpg_codegen`.
- Test tooling: `cmake --build build --target test_ttrpg`.
- Registered through CTest: `ctest --test-dir build --output-on-failure`.
- CLI integration tests: `ctest --test-dir build -R '^TtrpgCliTest$' --output-on-failure`; building `test_ttrpg` also builds the CLI they invoke.
- `ttrpg`, `ttrpg_codegen`, and `test_ttrpg` opt out of clang-tidy in CMake; IDE case-style/include diagnostics in this subtree may be spurious.

## Source Layout
- `include/ttrpg/schema_ast.h`: parsed AST declarations (`TEnumDecl`, `TVariantDecl`, `TClassDecl`, `TFieldDecl`, `TSchemaModule`, `EContainer`).
- `include/ttrpg/parser.h`, `src/parser.cpp`: tokenization and recursive-descent parser. Expressions are reconstructed with token locations and parsed by shared `expr::Parse`; balanced delimiters preserve nested object initializers and method bodies.
- `include/ttrpg/module.h`, `src/module.cpp`: file parsing, recursive imports, duplicate type checks, circular import detection, symbol table, and canonical `SourcePaths` for dependency tracking. Distinct files sharing a module stem are rejected.
- `include/ttrpg/conventions.h`, `src/conventions.cpp`: naming and type lowering (`PascalToSnake`, `CppMemberType`, `ScalarParseExpr`, `DefaultExprToCpp`, `VariantKindEnum`).
- `include/ttrpg/analyze.h`, `src/analyze.cpp`: legacy computed-default validation and lowering.
- `include/ttrpg/program.h`, `src/program.cpp`: typed rule expressions, method validation, purity/lvalues, query dependencies, initialization order, and checked lowering.
- `include/ttrpg/abilities.h`, `src/abilities.cpp`: ability symbols, reusable feature grant graphs, registration names, and typed traversal of stored grant sources.
- `include/ttrpg/emit_rule_class.h`, `src/emit_rule_class.cpp`: rule classes, methods, entity lifecycle, snapshots, and recursive JSON emission.
- `include/ttrpg/cpp_writer.h`, `src/cpp_writer.cpp`: generated-output indentation/braces.
- `include/ttrpg/emit.h`, `src/emit.cpp`: header/implementation emission.
- `cli/main.cpp`: argument parsing and orchestration only.
- `cmake/TtrpgCodegen.cmake`: `ttrpg_generate(SCHEMA ... HEADER ... SOURCE ... [DEPFILE ...] [TARGET ...])`; schema paths resolve against the source directory and output paths against the binary directory. `TARGET` must already exist and places generated-source properties in its directory, so `SKIP_LINTING` applies when generation is declared in a child directory.
- `tests/`: writer, conventions, parser, analyzer, and golden-output tests.

## Language
- Top-level forms: `import`, `enum`, `variant`, `class`, `GameEntity`, `action`, `reaction`, `feature`, and named instances (`TActor Wolf { ... }`). The trailing semicolon after an instance is optional.
- Field types: `int`, `bool`, `string`, `BoundedQuantity`, `Resource`, `Feature`, or schema-declared enum/class/variant. `Feature` is a reserved built-in name and lowers to `TFeatureSelection`.
- Containers:
  - `set<Enum>` lowers to `std::set`.
  - `set<Variant>` lowers to `TVariantMap`.
  - `collection<Class>` and `collection<Feature>` lower to `TIdCollection`.
  - `map<Enum, V>` lowers to `std::map`; JSON object keys are parsed through the generated enum `FromString`.
- Defaults: int literals, `true`/`false`, enum identifiers, `max_int`, `min_int`.
- Stored defaults run once during creation, after their dependencies. `derive` evaluates on every read. Initial creation overrides do not turn defaults into live formulas.
- Rule expressions support `@self`, stored/derived member reads, typed object literals, strings, int/bool operators, pure `min`/`max`, and query calls. Arithmetic checks overflow and division by zero.
- Enum values support equality and inequality against the same enum type. Bare enum literals resolve from their typed context or a unique declaration; ambiguous untyped literals fail validation. Methods and queries accept enum parameters alongside primitive parameters.
- Maps provide the pure lookup `@self.Values.get(Key, Default)`. It returns a copy of the stored value or the supplied fallback, evaluates the pure fallback eagerly, and never inserts a missing key. Keys and fallbacks are type checked. Lookup results support field reads and query chains such as `@self.Values.get(Key, TValue {}).Bonus(Level)`, including through local copies. A returned value is not a persistent mutation target or grant source.
- Collection defaults and object/named-instance initializers accept lists such as `[Training, Keen]` and `[TWeapon { Name = "Sword"; }]`; empty lists and trailing commas are supported. Elements are type checked and receive stable collection IDs during construction.
- `action Strike;` and `reaction Opportunity;` declare reusable ability identities. `feature Training { grant Opportunity; }` groups actions, reactions, and other features; unknown definitions, duplicate declarations/grants, and cyclic groups fail validation.
- Classes and entities grant definitions (`grant BasicActions;`) or stored sources (`grant @self.Features;`, `grant @self.Weapons;`, `grant @self.Build.SelectedFeature;`). A dynamic source must be a scalar or collection of `Feature` or a class declaring grants. Derived values, queries, indexes, and primitive values cannot supply a stable grant source.
- A `Feature` field or collection element initializes from a feature declaration symbol. Action and reaction symbols cannot be used as feature selections. Empty scalar selections default to no grant. Feature selections are not variant payloads, set elements, or map values.
- `method ReturnType Name(Type Argument) { ... }` supports `let`, `require`, assignment/compound assignment, method calls, `return`, and `if (...) { ... } else { ... }`.
- `query` methods are pure and cannot write persistent fields or call mutating methods. Query dependencies participate in default ordering and cycle checks.
- Persistent field assignments and method calls automatically use the generated execution context. Local values are copied and local assignments use ordinary C++; context, guarded access, IDs, and transaction declarations are absent from the author syntax.
- `derive` fields are class-only storage-less getters. They require an initializer, are skipped in `FromJson` and `GetAst`, and can be DSL-exposed if `int`/`bool`.
- Variant alternatives reject derived fields and non-constant computed initializers.
- Type names are verbatim C++ names; the generator never adds `T` or `E` prefixes.

## Naming (automatic, no per-field attributes)
- A PascalCase field `FooBar` maps to: class member `FooBar_`, variant-payload member `FooBar` (public, no underscore), getter `FooBar()`, JSON key `foo_bar` (`PascalToSnake`), DSL property `$obj.foo_bar`, AST key `foo_bar`.
- Variant kind enum name = strip leading `T`, prefix `E`, suffix `Kind` (e.g. `TWeaponTrait` → `EWeaponTraitKind`); payload struct = type + alt name (e.g. `TWeaponTraitFatal`).

## Generated Shape
- Enums emit `enum class`, `ToString`, and `<Name>FromString`.
- All classes emit guarded/const getters, `Create(TInitialValues)`, `FromJson`, `GetAst`, `TIsAstRecursive`, and `RegisterDslProperties`.
- Rule classes (methods, entities, new expression syntax, or fields containing rule classes) also emit `ToJson` and `Restore`. Snapshot restoration requires every stored field and bypasses creation defaults.
- Classes with grants or `Feature` fields also use rule-class generation and emit `VisitGrantedAbilities`. Each enumeration reads current fields/collection entries. Source paths use snake_case field names and stable collection entry IDs; static grants preserve the owner source, and grant sites contribute separate provenance.
- Every module emits `Register<PascalCaseStem>Abilities(TAbilityCatalog&)`, calls the registrars for its direct imports, and registers definitions using `<canonical-module-stem>.<Name>` keys. Canonical import stems preserve symlink identity; conflicting generated registrar names are rejected. The caller supplies the catalog and actor identity.
- `GameEntity` adds a generated `TEntityId`; factories create distinct identities, C++ copying is disabled, moving transfers identity and clears the source, and restoration reserves the saved ID.
- Entity ASTs use the identity already registered in `TAstContext`, falling back to `entity#<Id>` when no contextual identity exists. Repeated traversal keeps the same identity, and corresponding entities in independent worlds can compare under matching contextual paths. JSON snapshots retain the numeric entity ID. Generated owned-field, resource, map-value, and collection-entry identities extend the same contextual path.
- Named instances emit `Create<Name>()` factories instead of shared mutable globals. Two calls create independent values/entities.
- Variants emit a kind enum, one payload struct per alternative, wrapper class with `std::variant` payload, `Kind()`, `Payload()`, `TryGet<T>()`, `FromJson`, and `GetAst`.
- `set<Variant>` uses `TVariantMap<Kind, Variant>`; query with `Has(kind)` or `Get<TAlt>()`.
- Enum/variant JSON tokens use the identifier spelling verbatim, such as `"Finesse"` or `"D8"`.
- Generated output is formatted only by `TCppWriter`; there is no external clang-format pass. Golden tests lock exact text output.

## Factory/Engine Integration
- Class fields load from either by-name string refs through `TGameObjectFactory` or inline JSON objects through `T::FromJson`.
- New generated classes used by-name register with `factory.RegisterType<T>("json_type")` before `AddSource`; no dedicated storage member, reader method, or factory switch is needed. Existing built-in registrations remain supported.
- Inline generated classes need no registration. Their generated by-name branch compiles and reports missing registration only if invoked.
- Generic definitions retain immutable JSON and are constructed through `T::FromJson(definition, currentFactory)`, so factory moves/copies do not bind reference resolution to an old factory address. Registered JSON types validate their envelope at load time and their fields through `FromJson` when constructed.
- Recursive by-name construction reports the reference cycle. Construction guards are scoped to the current thread, factory, C++ type, and definition id, and unwind when loading fails.
- Variants load eagerly through `FromJson` and need no factory storage.
- Register each schema with `ttrpg_generate` in `pf2e_engine/src/inventory/CMakeLists.txt`; its depfile tracks transitive imports without a handwritten dependency list.
- Add generated sources to `pf2e_engine`, outputs to `pf2e_engine_generated_headers`, and call `TClass::RegisterDslProperties()` from `pf2e_engine/src/dsl/builtins.cpp` for generated classes with DSL-visible fields.

## Tests
- `test_writer`: `TCppWriter` scopes, indentation, and brace balance.
- `test_conventions`: naming/type-lowering conventions.
- `test_parser`: grammar, imports, duplicate types, circular imports.
- `test_analyze`: computed defaults, derives, ordering, and invalid expressions.
- `test_abilities`: declaration graphs, stored grant paths, imported registration, feature/list lowering, source escaping, and invalid source/type diagnostics.
- `test_golden`: exact generated output for `basic`, `variant`, `computed`, and `collection` fixtures.
- Engine tests are the end-to-end gate after generator changes.

## CLI
- Generate with `ttrpg_codegen --schema module.ttrpg --out-h module.h --out-cpp module.cpp [--depfile module.d]`.
- `ttrpg_codegen --schema module.ttrpg --check` validates header and implementation emission for every imported module without creating files; it cannot be combined with output paths.
- Generation renders both outputs before writing, stages files beside their destinations, and restores existing outputs if publication fails. Schema or emission errors leave existing output contents and timestamps unchanged.
- `--depfile` writes Make-compatible dependencies for both outputs, including the primary schema and every transitive import. The CMake helper enables `DEPFILE`; Makefile generators require CMake 3.20 or newer.

## Pitfalls
- Keep generated output deterministic.
- The legacy initializer path keeps the existing expression subset; the typed rule path supports the expanded language. `$` expressions remain the separate runtime action DSL.
- Methods are synchronous. Do not insert continuation yields or user prompts into generated methods: suspension needs its own action executor.
- Mutable nested fields have stable addresses while transformation history exists. Object/collection replacement and element indexing are rejected until stable paths are implemented. Method parameters currently accept primitive and enum value types.
- Entity-valued fields are rejected until entity-reference storage is implemented; no explicit `ref` keyword is planned. Creating entities inside methods/queries is also rejected until allocation can participate in the transformation journal. Named instance factories are initial-world construction.
- Action/reaction declarations currently provide identities and grant traversal; their execution bodies, costs, conditions, and reaction triggers are bound by the runtime. Compiling those bodies, contextual modifiers, build choices, and fully generated `TCreature` remain subsequent migration steps.
- `Statistic` in `hitpoints.ttrpg` currently implements Base/Value only. PF2e modifier stacking is not implemented by that value class.
- `BoundedQuantity` is scalar only; `set<BoundedQuantity>` is rejected.
- `Resource` is a runtime-supported scalar built-in (lowers to `TResource`), like `BoundedQuantity`: scalar-only, supports an int default, and is usable as a `map<Enum, Resource>` value; not DSL-exposed yet.
- `collection<T>` is supported for schema-declared classes and built-in `Feature`; engine schemas use both.
- `map<T, V>` keys must be schema-declared enums; the first engine use is `map<EDamageType, int>`.
- DSL exposes only generated `int`/`bool` scalar getters today; widening needs new `TDslValue` alternatives.
- No inheritance or author-facing `optional` field shapes yet.
- Legacy nested classes serialize through their schema getters. A legacy class containing its own collection still needs a snapshot restoration path before it can participate in rule-class snapshots.
- Variants are closed by design; adding an alternative should force visit sites using `overloaded` to handle it.
