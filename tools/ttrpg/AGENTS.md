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
- `include/ttrpg/events.h`, `src/events.cpp`: typed feature-event symbols, generated tag names, owner/result/parameter validation, and handler declaration validation.
- `include/ttrpg/emit_rule_class.h`, `src/emit_rule_class.cpp`: rule classes, methods, entity lifecycle, snapshots, and recursive JSON emission.
- `include/ttrpg/action_flow.h`, `src/action_flow.cpp`: typed action frames, basic blocks, branch/loop/choice exits, and direct C++ dispatcher rendering.
- `include/ttrpg/emit_action.h`, `src/emit_action.cpp`: compiled action executors, bound resource costs, and module rule registration.
- `include/ttrpg/emit_source.h`, `src/emit_source.cpp`: generated traversal of stored class source providers and their ancestors.
- `include/ttrpg/cpp_writer.h`, `src/cpp_writer.cpp`: generated-output indentation/braces.
- `include/ttrpg/emit.h`, `src/emit.cpp`: header/implementation emission.
- `cli/main.cpp`: argument parsing and orchestration only.
- `cmake/TtrpgCodegen.cmake`: `ttrpg_generate(SCHEMA ... HEADER ... SOURCE ... [DEPFILE ...] [TARGET ...])`; schema paths resolve against the source directory and output paths against the binary directory. `TARGET` must already exist and places generated-source properties in its directory, so `SKIP_LINTING` applies when generation is declared in a child directory.
- `tests/`: writer, conventions, parser, analyzer, and golden-output tests.

## Language
- Top-level forms: `import`, `enum`, `variant`, `class`, `GameEntity`, `action`, `reaction`, `feature`, `event`, and named instances (`TActor Wolf { ... }`). The trailing semicolon after an instance is optional.
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
- Set initializers reuse list syntax: `set<Enum>` accepts enum values and `set<Variant>` accepts alternatives such as `[Agile, Fatal { Die = D10; }]`. Known duplicate variant kinds fail compilation. Dynamic variant values are evaluated once each, checked for duplicate kinds during construction, and inserted only after validation. Enum sets retain ordinary set deduplication.
- Maps initialize contextually as `{ Martial = TValue { Rank = Expert; }; }`; keys are enum members, independent of local/field names, and duplicate or unknown keys fail validation. Anonymous `{ Field = Value; }` initializers also construct classes or variant payloads when their type is supplied by the surrounding expression.
- Enum sets expose the pure `has(Value)` query. Variant sets expose `has(Alternative)` and `get(Alternative, Fallback)`, for example `Weapon.Traits.get(Fatal, { Die = D6; }).Die`. Lookup returns a copied payload or the eagerly evaluated fallback. Payload fields use ordinary DSL member access; generated payload type names, pointers, and null checks are absent from the author syntax.
- Named ordinary class values are reusable expressions: `TWeapon Blade { ... }` followed by `Weapons = [Blade, Blade]` calls the generated factory twice and produces independent values. Named values also work through imports and class field defaults. Construction validation follows explicit values, omitted-field defaults, and the typed bodies of called queries, including queries on another class. It rejects cycles and respects overrides, parameter shadowing, and typed enum literals. GameEntity factories cannot be used as value expressions.
- `action Strike;` and `reaction Opportunity;` declare reusable ability identities. `feature Training { grant Opportunity; }` groups actions, reactions, and other features; unknown definitions, duplicate declarations/grants, and cyclic groups fail validation.
- `action Mend for TActor { cost @self.Effort = 1; choose TActor Target where Target.IsAlive(); let Amount = roll(1, 6); Target.Health.Heal(Amount); }` supplies a compiled action body. The owner and chosen types must be `GameEntity` types; the runtime binds them to checked entity references without author-visible reference types.
- Action costs precede executable statements, target direct or nested stored `Resource` fields of `@self`, and use nonnegative constant integer expressions. Costs bind generated getters rather than engine resource-kind enums.
- `requires Condition;` declares a pure action precondition in the initial prefix, where it can alternate with `cost`. It filters available options and is checked again immediately before execution and payment. It is not a continuing invariant after payment. A first `choose` immediately after that prefix adds an existence check using its predicate; choices following executable statements are not evaluated speculatively. Existing `require` statements remain assertions at their position in the executable body.
- Preconditions read the current source provider. The paid action and its choice predicates use the source snapshot prepared before payment. Resource budgets are readable through the pure `@self.Actions.Count()` query; resource mutation still uses declared costs or runtime transformations.
- `action Strike for TActor using TWeapon Weapon { ... }` binds the action's typed source. The type must be an ordinary class. Runtime preparation snapshots the nearest matching provider ancestor before resource payment. Reads use the immutable snapshot; actor and current grant/source validity are checked before every statement and continuation, even if the source is not read again. The binding is copied into choice captures. A normal `let Copy = Weapon` creates a value copy and does not expose mutable source access.
- Action bodies reuse typed `let`, assignment, `require`, `if`, synchronous method calls, and void `return`. Branches and `while (Condition) { ... }` loops may suspend with `choose`, including nested branches and loops. A return exits the entire action. Conditions and selection predicates are pure; local declarations remain lexical and cannot shadow an active local.
- `choose TActor Target;` selects a checked mutable entity. Explicit sequences use `choose EDecision Decision from [Continue, Finish];`, `choose int Amount from [1, 2];`, or `choose Position Next from adjacent_positions(position(@self)) where can_occupy(@self, Next);`. Primitive, enum, and Position choices require `from`; entity choices optionally accept an entity sequence. Sources evaluate once per prompt, and the pure `where` predicate is checked both when offered and when received. Selecting from read-only world participants gives the selected entity a checked mutable binding without changing the original sequence.
- `roll(Count, Size)` is an effectful action expression. Arithmetic and method-argument expressions remain pure; use a local to capture a roll first. Loops may assign a standalone roll directly or use it as the right side of a local compound assignment.
- Actions, feature hooks, and their predicates/preconditions have implicit access to a checked read-only world. `entities(TActor)` takes a declared GameEntity type and returns a sequence of read-only bindings. `for Ally in entities(TActor)` copies bindings, not entities; aliases remain read-only, including after choice capture. The sequence fixes participant identities at evaluation time, while each read validates identity/membership and observes current fields. Action `@self` and explicitly chosen targets remain mutable through transformations.
- Pure world queries are `grid_distance(A, B)`, `same_team(A, B)`, `has_line_of_effect(A, B)`, `position_x(A)`, and `position_y(A)`. All arguments must be checked GameEntity bindings from the current world. `grid_distance` explicitly means Chebyshev distance in map cells; it does not select a game system's distance policy. Ordinary class methods/queries and field defaults cannot access an implicit world. Native calls without a world can still evaluate nonspatial preconditions and hooks, but world operations reject the missing context.
- `event int HealingBonus for TActor(TActor Target, int Amount);` declares a typed event. The owner must be a `GameEntity`. Results accept primitive, enum, or ordinary class values; parameters additionally accept implicit checked GameEntity bindings. Event names cannot conflict with types, ability symbols, built-in functions, or another event. Generated `TEvent<Name>` tags cannot collide with author-declared types.
- `feature Helpful { on HealingBonus { return @self.Level + Target.Level + Amount; } }` binds a pure handler to a feature, including a feature with no grants. The handler inherits parameter names and types from the event and uses the event receiver as `@self`. Every path must return the declared result. Queries and value construction are allowed; RNG, choice, mutation, nested event dispatch, and entity allocation are rejected. One feature can handle each event once.
- `HealingBonus(@self, Target, Amount)` evaluates the receiver's current feature sources and returns a temporary sequence of contribution values. Event calls currently require an action context. The receiver is explicit and can be a chosen entity; it need not be the action actor. These evaluations are pure and eager, without implicit bonus stacking or mutation of action state.
- `for Bonus in HealingBonus(@self, Target, Amount) { Amount += Bonus; }` synchronously iterates copied contribution values. A prior `let Bonuses = HealingBonus(...);` can store the sequence. Methods, queries, and hooks can also iterate stored `collection<T>` values by copying their elements, such as `for Weapon in @self.Weapons { Reach = max(Reach, Weapon.Reach); }`. Action loop bodies support `roll`, nested loops, and branches, but cannot suspend with `choose`. Loop locals do not escape; outer local assignments survive the loop and subsequent choice capture. A return inside the loop does not imply the sequence is nonempty.
- Synchronous ordinary queries borrow a direct stored `@self.Collection` for iteration instead of copying the entire collection. Elements remain value copies. Temporary, derived, nested, and local collection expressions retain owned snapshots, as do action, hook, and mutating-method loops; the generated borrow never extends a reference returned from a temporary object.
- Classes and entities grant definitions (`grant BasicActions;`) or stored sources (`grant @self.Features;`, `grant @self.Weapons;`, `grant @self.Build.SelectedFeature;`). A dynamic source must be a scalar or collection of `Feature` or a class declaring grants. Derived values, queries, indexes, and primitive values cannot supply a stable grant source.
- A `Feature` field or collection element initializes from a feature declaration symbol. Action and reaction symbols cannot be used as feature selections. Empty scalar selections default to no grant. Feature selections are not variant payloads, set elements, or map values.
- `method ReturnType Name(Type Argument) { ... }` supports `let`, `require`, assignment/compound assignment, method calls, `return`, and `if (...) { ... } else { ... }`.
- Method and query parameters accept primitives, enums, and ordinary classes by value. Class parameters are read-only in the DSL; they can be inspected and passed to queries but cannot be mutated through guarded operations. GameEntity method parameters remain unsupported; actions and events use their dedicated checked binding semantics.
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
- All classes emit `VisitAbilitySources(visitor, path)` over themselves, stored class fields, and class collection entries. Paths use generated snake_case fields and stable entry IDs. Visitors may provide `ShouldVisit(path)` to prune unrelated branches; plain visitors retain complete traversal. The runtime resolves the nearest matching ancestor of a granted source and never stores these temporary raw pointers in action options.
- Rule classes (methods, entities, new expression syntax, or fields containing rule classes) also emit `ToJson` and `Restore`. Snapshot restoration requires every stored field and bypasses creation defaults.
- Both legacy and rule-class JSON readers require integer JSON values for `int`, check signed and unsigned bounds before narrowing, and reject fractional, boolean, or otherwise noninteger values. Literal defaults, map values, variant payloads, and snapshots use the same checked reader. A module emits one internal `ReadGeneratedRuleInteger` helper when needed; calls are globally qualified so member names cannot shadow it, and a conflicting author type is diagnosed before generation.
- Classes with grants or `Feature` fields also use rule-class generation and emit `VisitGrantedAbilities`. Each enumeration reads current fields/collection entries. Source paths use snake_case field names and stable collection entry IDs; static grants preserve the owner source, and grant sites contribute separate provenance.
- Every module emits `Register<PascalCaseStem>Abilities(TAbilityCatalog&)`, calls the registrars for its direct imports, and registers definitions using `<canonical-module-stem>.<Name>` keys. Canonical import stems preserve symlink identity; conflicting generated registrar names are rejected. The caller supplies the catalog and actor identity.
- Every module also emits `Register<PascalCaseStem>Rules(TAbilityRules&)`, registers its catalog and imported rules, and installs its compiled executors. Each executor uses a static shared action object so repeated registration and diamond imports remain idempotent.
- Source-bound actions register a static shared immutable `TAbilityExecution` with typed `Requirement` and `Prepare` functions. `RequiredSource` checks eligibility and `PrepareActionSource` captures the provider before costs. The body obtains that prepared snapshot with `ActionSource`, never a mutable collection reference.
- Event tags define `TOwner`, `TResult`, `TArguments`, `FHandler`, and the module-qualified `kId`. GameEntity arguments lower to `TGeneratedEntityBinding<T>`; ordinary parameters are values. Generated handlers register static shared function objects through `rules.Events.Register<TEvent>`, preserving idempotence. `DispatchFeatureEvent<TEvent>` returns tagged contribution snapshots; generated loops copy each entry's `Value`.
- Each dispatch recollects the receiver's current `Features()`, deduplicated by feature ID plus source path while retaining provenance. Distinct source instances contribute separately. Evaluation does not mutate a shared contribution buffer. Contribution snapshots and plain choice-value sequences are distinct typed sequence kinds and cannot be assigned to one another. A stored sequence is a snapshot at its invocation point; resuming after a later choice preserves that snapshot, while reexecuting the invocation recollects current sources. Read-only runtime bindings prevent a handler from obtaining mutable entity access.
- Action bodies generate direct C++ using `actions/generated_action.h`, never serialized runtime DSL. A typed frame stores locals in optional slots and an iterative switch dispatcher executes basic blocks. Synchronous branches and loop backedges do not recurse. Lexical scope exits reset their slots, so expired entity bindings are neither retained nor validated in later blocks. Frame aliases and synchronous action locals share one generated-name allocator, including suffix collisions with authored names. Dispatcher, frame, and run names avoid declared types, variant payloads/kind enums, and event tags. Typed references in actions and preconditions are globally qualified before emission, including constructor calls, casts, templates, and frame storage, so author types can use names such as Context, frame, pc, and Run.
- `SelectActionValue` and `SelectActionEntity` return immediate selections directly; only a suspended selection invokes its continuation callback. Each callback captures an immutable frame snapshot, copies it afresh, installs the selected value, and invokes the dispatcher. No frame owns a callback or shares mutable continuation state across resume/revert/replay. Actor/source validity and live chosen identities are checked before every statement; selection predicates run when offered and when chosen. Actor, world, and prepared immutable source bindings are recovered from the action context on dispatcher entry.
- The control-flow IR separates synchronous statements from Jump, Branch, Choose, and Return exits; future yielding activities can add a continuation edge without serializing rule code or changing the author-visible ownership model.
- `actions/generated_world.h` supplies the world facade. Generated action predicates capture it by value; generated event `FHandler` signatures receive `const TGeneratedWorldView&` before the owner and arguments. A compiled action with source or preconditions registers a stable full `TAbilityExecution` whose requirement receives world, actor, source path, and optional trigger. The requirement performs only pure reads; preparation and payment remain runtime responsibilities.
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
- `test_action_flow`: nested choice grammar, typed value sequences, branch joins, loop backedges, optional-slot liveness and cleanup, pure preflight boundaries, and lexical/type/impurity diagnostics.
- `test_actions`: typed action grammar, arbitrary resource paths and constant costs, entity bindings, continuation capture shape, pure predicates, generated registration, and unsupported-scope diagnostics. Compiled engine action fixtures cover runtime choice, RNG, rollback, and replay behavior.
- `test_events`: event/hook grammar and validation, typed tags and imported events, pure entity/class parameters, class contributions, loop copies and captures, and diagnostics for unsupported dispatch or suspension.
- `test_sources`: source snapshot captures and preflight emission, schema source traversal, value-class parameters, collection queries, typed map/set/variant initialization, payload reads, imported named values, factory cycles, and generated-name collisions.
- `test_world`: implicit world scopes, typed entity sequences, read-only alias/capture validation, world query types, action precondition purity/placement, leading-choice preflight boundaries, and resource budget reads. Compiled engine fixtures cover missing/foreign worlds, option filtering, payment order, and resume behavior.
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
- Mutable nested fields have stable addresses while transformation history exists. Object/collection replacement and element indexing are rejected until stable paths are implemented. Source bindings and collection loops provide read-only snapshots, not journaled element mutation.
- Entity-valued fields are rejected until entity-reference storage is implemented; no explicit `ref` keyword is planned. Creating entities inside methods/queries is also rejected until allocation can participate in the transformation journal. Named instance factories are initial-world construction.
- Bare action/reaction declarations still permit runtime executors. Compiled action bodies currently support resource costs, pure preconditions and world queries, entity choice, RNG, typed synchronous writes, and pure feature events. Reaction bodies, yielding event handlers and `for` loops, reusable yielding activities, contextual modifier policies, and system-specific spatial policies remain subsequent migration steps.
- `Statistic` in `hitpoints.ttrpg` currently implements Base/Value only. PF2e modifier stacking is not implemented by that value class.
- `BoundedQuantity` is scalar only; `set<BoundedQuantity>` is rejected.
- `Resource` is a runtime-supported scalar built-in (lowers to `TResource`), like `BoundedQuantity`: scalar-only, supports an int default, and is usable as a `map<Enum, Resource>` value. Typed rules can read `Count()`; the legacy runtime DSL has no resource property exposure yet.
- `collection<T>` is supported for schema-declared classes and built-in `Feature`; engine schemas use both.
- `map<T, V>` keys must be schema-declared enums; the first engine use is `map<EDamageType, int>`.
- DSL exposes only generated `int`/`bool` scalar getters today; widening needs new `TDslValue` alternatives.
- No inheritance or author-facing `optional` field shapes yet.
- Legacy nested classes serialize through their schema getters. A legacy class containing its own collection still needs a snapshot restoration path before it can participate in rule-class snapshots.
- Variants are closed by design; adding an alternative should force visit sites using `overloaded` to handle it.
