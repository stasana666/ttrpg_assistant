# `ttrpg` Schema Language Tooling

Canonical project instructions. CLAUDE.md redirects here.

## Purpose
- C++23 static library `ttrpg` plus the thin CLI `ttrpg_codegen`.
- Turns one `.ttrpg` module and its imports into generated `*.h` / `*.cpp`.
- The schema is the source of truth for generated C++ types, AST serialization and compiled rule bodies.

## Build/Test
- `cmake --build build --target ttrpg_codegen test_ttrpg`, then run `./build/tools/ttrpg/tests/test_ttrpg`.
- CLI integration: `ctest --test-dir build -R '^TtrpgCliTest$' --output-on-failure`.
- Engine tests are the end-to-end gate after generator changes. Generated output must stay deterministic.
- `ttrpg`, `ttrpg_codegen` and `test_ttrpg` opt out of clang-tidy.

## Source Layout
- `schema_ast.h` holds parsed declarations. `parser.h` / `parser.cpp` tokenize and parse; expressions go through the shared `expr::Parse`.
- `module.h` / `module.cpp` load files and imports, reject import cycles, duplicate types and distinct files sharing a stem, and build the symbol table and `SourcePaths`. They also hold helpers shared by every declaration kind: `AllModules`, `FindClass`, `IsImportedBy`, `CollectDeclarations`, `ResolveDeclaration` and the `AnyNode` walkers.
- `conventions.*` names and lowers types. `program.*` type-checks and lowers every class, field default, method, action, activity, reaction, hook and constructor.
- `abilities`, `activities`, `triggers`, `records`, `relations`, `events`, `constructors` and `callable_graph` validate their declaration kind.
- `emit.cpp` drives emission and runs `ValidateNames`, which rejects every duplicate or colliding declared or generated top-level name. The other `emit_*` files and `action_flow` render each construct; `cpp_writer` owns indentation.
- `cli/main.cpp` parses arguments and writes outputs. `cmake/TtrpgCodegen.cmake` provides `ttrpg_generate(SCHEMA HEADER SOURCE [DEPFILE] [TARGET])`; `TARGET` applies `SKIP_LINTING` in the consuming target's directory.
- `tests/` has one `test_<area>.cpp` per language area over `compile_helpers.h`, golden output tests for the `basic`, `variant`, `computed` and `collection` fixtures, and `test_cli.py`.

## Language: Data
- Top-level forms: `import`, `enum`, `variant`, `class`, `GameEntity`, `action`, `reaction`, `feature`, `event`, `activity`, `trigger`, `record`, `relation`, and named instances such as `TActor Wolf { ... }`.
- Field types: `int`, `bool`, `string`, `BoundedQuantity`, `Resource`, `Feature`, or a declared enum, class or variant. Type names are used verbatim as C++ names; the generator adds no prefixes.
- Containers: `set<Enum>` lowers to `std::set`, `set<Variant>` to `TVariantMap`, `collection<Class>` and `collection<Feature>` to `TIdCollection`, and `map<Enum, V>` to `std::map`. `BoundedQuantity` and `Resource` are scalar only.
- Stored defaults run once at creation in dependency order. `derive` fields are storage-less getters evaluated on every read and skipped by AST.
- Expressions support `@self`, member reads, literals, strings, overflow-checked int arithmetic, bool operators, enum equality, pure `min`/`max`, query calls, typed object literals `T { Field = Value; }`, anonymous `{ Field = Value; }` when the type is known, list literals, and map literals keyed by enum members. Bare enum literals resolve from their typed context or a unique declaration.
- Pure container queries: `map.get(Key, Fallback)`, `set.has(Value)` for enums, `set.has(Alternative)` and `set.get(Alternative, Fallback)` for variants, and `collection<Feature>.has(Feature)`, which checks direct selections only. Lookups return copies and evaluate the fallback eagerly.
- A named value such as `TWeapon Blade { ... }` is a factory: each use builds an independent value. GameEntity factories cannot be used as values.
- Named constructors take typed parameters, for example `TActor Fighter(int Level = 1, collection<TWeapon> Equipment = [Blade], choice Feature Feat from Pool.Values when Condition = Default) { requires Level >= 1; Weapons = Equipment; }`. Parameters resolve left to right and see only earlier parameters. A false `when` disables a choice and rejects overrides, and unresolved dependencies leave later choices Pending. Constructor expressions are pure: no `@self`, world, RNG, mutation or activities.
- Parameterized constructors generate `T<Name>Options`, `Create<Name>`, `Describe<Name>` and `Validate<Name>`, all through one resolver; validation finishes before any entity identity is allocated.

## Language: Methods and Grants
- `method` bodies support `let`, `require`, assignments, method calls, `return` and `if`/`else`. `query` methods are pure. Parameters accept primitives, enums and read-only ordinary classes.
- Persistent writes and method calls go through the generated execution context and journal automatically.
- Methods, queries and hooks may iterate stored collections with `for Item in @self.Items { ... }`; elements are copies.
- `action Name;` and `reaction Name;` declare ability identities, and `feature Name { grant Other; }` groups them. Unknown, duplicate or cyclic grants are rejected.
- Classes grant definitions (`grant BasicActions;`) or stored sources (`grant @self.Features;`, `grant @self.Weapons;`). A source must be a stored `Feature`, a feature collection, or a class that declares grants.

## Language: Actions, Activities and Reactions
- `action Name for TOwner [using TSource Item] [after Record Previous] { ... }` compiles to direct C++. The owner and chosen entities must be `GameEntity` types and are bound through checked runtime references.
- The body starts with `cost @self.Resource = N;` and pure `requires Condition;` statements. Costs are constant and paid once before the body. Requirements filter options and are rechecked before payment, not after.
- A leading `choose`, optionally after pure `let` lines, adds a preflight existence check, so an empty candidate set hides the action before payment. Preflight never rolls, mutates, prompts or calls activities.
- Bodies support `let`, assignment, `require`, `if`, `while`, synchronous method calls, `return`, `roll(Count, Size)` and `choose`. Branches and loops may suspend on `choose` and keep their locals across replay. Capture `roll` in a local before using it in arithmetic.
- `choose TActor Target [from Sequence] [where Predicate];` selects a mutable entity; values and positions need `from [...]`. Predicates are pure and checked both when offered and when received.
- `choose TItem Item from @self.Items [where ...];` or `from sources(@self.A, @self.B)` selects a stored source. The selection is an immutable snapshot whose owner and entry are revalidated before later statements and resumes. Source-derived class values cannot be materialized, returned or passed to mutating methods.
- `activity Result Name for TOwner(Params) { ... }` is a reusable yielding body, called as a standalone statement or a `let` root with pure arguments. Activities share the caller's actor, journal and source validation and have no costs of their own. Non-void bodies must return on every path.
- `trigger TState Name(TState Pending, ...) [once per activity];` declares a reaction window opened with `let Next = Name(Pending, ...);`. `reaction Counter for TOwner [using TSource Item] on Name, Other { ... }` uses the action grammar and returns the state on every path; subscribed triggers must have identical signatures. `once per activity` limits each reactor to one use per caller frame. Cycles across activities and windows are rejected.
- `record Name for TOwner using TSource Item(Params);` declares a completion payload. `publish Name(...)` must be the last top-level statement of a root action. `after Name Previous` requires the actor's current record, and `same_source(A, B)` compares bound source identity.
- `relation TValue Name for TOwner(TKey Key);` stores a value per entity pair through `Name.has`, `Name.get(Owner, Key, Fallback)`, `Name.set(Owner, Key, Value[, Duration])` and `Name.erase`. Reads need a world; writes need an effectful body and a mutable owner. `seconds(int)` and `minutes(int)` build opaque durations.
- `event TResult Name for TOwner(Params);` declares a typed event; `feature F { on Name { return ...; } }` binds a pure handler. `Name(Receiver, ...)` returns the contributions of the receiver's current features and needs an action context; `for Bonus in Name(...)` folds them.
- Actions, hooks and predicates see a read-only world: `entities(T)`, `grid_distance` (Chebyshev cells), `same_team`, `has_line_of_effect`, `position`, `position_x`, `position_y`, `adjacent_positions`, `map_positions` (every cell, column by column), `contains_position` and `can_occupy`. `move` and `move_before_current_turn(Entity)` are effectful. `participants(T)` lists every participant of type `T` as mutable bindings; it is allowed only in effectful action, activity and reaction bodies, so area effects can change several creatures without a choice.

## Generated Shape
- Field `FooBar` becomes class member `FooBar_` or public variant-payload member `FooBar`, getter `FooBar()`, and key `foo_bar` in the AST. Variant `TWeaponTrait` gets kind enum `EWeaponTraitKind` and payloads such as `TWeaponTraitFatal`.
- Enums emit `enum class`, `ToString` and `<Name>FromString`. Every class emits getters, `Create`, `ResolveInitialValues`, `GetAst`, `VisitAbilitySources` and `VisitGrantedAbilities`. A field missing from `Create` options takes its declared default or a value-initialized value. Generated types have no JSON form.
- `GameEntity` classes carry a generated `TEntityId`, cannot be copied, and transfer identity on move.
- Each module emits `Register<Stem>Abilities` and `Register<Stem>Rules`, which register imports first. Definition IDs are `<stem>.<Name>`.
- Compiled bodies use a typed frame and an iterative block dispatcher. Suspended choices copy the frame, so immediate choices never grow the C++ stack.

## Engine Integration
- Register schemas with `ttrpg_generate` in `pf2e_engine/src/inventory/CMakeLists.txt`; depfiles track imports.

## CLI
- `ttrpg_codegen --schema module.ttrpg --out-h module.h --out-cpp module.cpp [--depfile module.d]`, or `--check` to validate every imported module without writing files.
- All outputs are rendered before any file is written, and each file is replaced through a temporary sibling.

## Pitfalls
- Methods are synchronous; suspension belongs only in action, activity and reaction bodies.
- Not supported yet: entity-valued fields, creating entities in methods, replacing journaled objects or collections whose children have history, element indexing, inheritance, and author-facing optional fields.
- `Statistic` in `hitpoints.ttrpg` implements Base and Value only, without PF2e modifier stacking.
- Variants are closed; adding an alternative should break every `overloaded` visit site until it is handled.
