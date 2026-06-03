# `ttrpg` — the `.ttrpg` schema language library

A small C++23 **library** (`ttrpg` static lib) plus a thin CLI (`ttrpg_codegen`)
that turns one `.ttrpg` schema module into a generated `*.h` / `*.cpp` pair. The
schema is the single source of truth: editing it regenerates the C++ type, JSON
loader, AST serialization, and DSL property registration on the next `make`.

```
ttrpg_codegen --schema <in.ttrpg> --out-h <gen.h> --out-cpp <gen.cpp>
```

**clang-tidy is disabled** on `ttrpg`, `ttrpg_codegen`, and `test_ttrpg`
([CMakeLists.txt](CMakeLists.txt) sets `CXX_CLANG_TIDY ""`); the code uses
camelCase locals (`externalStems`, `kindEnum`) and builds C++-as-text which the
project's tidy config would reject. IDE/clangd diagnostics on these files
(missing `ttrpg/*.h` or `parse/scanner.h`, "invalid case style") are spurious —
clangd lacks the target include paths and the tidy-off setting. Trust `make`.

## Source layout

A compiler-shaped pipeline: **frontend** (text → schema AST) → **semantic layer**
(naming/typing conventions) → **backend** (C++ emission), with a thin CLI on top.

| File | Role |
|---|---|
| [include/ttrpg/schema_ast.h](include/ttrpg/schema_ast.h) | The parsed module: `TEnumDecl` / `TVariantDecl` / `TVariantAlt` / `TFieldDecl` / `TClassDecl` / `TSchemaModule`, `EContainer`. |
| [include/ttrpg/parser.h](include/ttrpg/parser.h) + [src/parser.cpp](src/parser.cpp) | `ETok`, `Tokenize` (wraps `parse::TScanner`; raw-captures `= ... ;` initializers into an `InitExpr` token), recursive-descent `TParser` → `TSchemaModule`. Field initializers are parsed by the shared `expr::Parse` ([tools/common/expr/](../common/expr/)), not here. |
| [include/ttrpg/module.h](include/ttrpg/module.h) + [src/module.cpp](src/module.cpp) | `ParseFile`, `LoadAll` (transitive imports, cycle-detected), the cross-file symbol table (`ETypeKind`, `TTypeInfo`, `TLoadedSchemas`). |
| [include/ttrpg/conventions.h](include/ttrpg/conventions.h) + [src/conventions.cpp](src/conventions.cpp) | The semantic layer: `PascalToSnake`, `FieldKindOf`/`EFieldKind`, `CppMemberType`, `VariantKindEnum`, `ScalarParseExpr`/`LoadFieldCall`, `DefaultExprToCpp`, `DeriveSiblingInclude`, … |
| [include/ttrpg/analyze.h](include/ttrpg/analyze.h) + [src/analyze.cpp](src/analyze.cpp) | Field-initializer analysis: `InitIsComputed` (constant vs computed default), `FieldInitOrder` (topological evaluation order + cycle / bad-reference detection), `InitExprToCpp` (lower an expression to C++). |
| [include/ttrpg/cpp_writer.h](include/ttrpg/cpp_writer.h) + [src/cpp_writer.cpp](src/cpp_writer.cpp) | `TCppWriter` — owns indentation/braces (see below). |
| [include/ttrpg/emit.h](include/ttrpg/emit.h) + [src/emit.cpp](src/emit.cpp) | `EmitHeader` / `EmitImpl` and the per-construct emitters, driving the writer. |
| [cli/main.cpp](cli/main.cpp) | Arg parsing + orchestration only (~90 lines). |
| [tests/](tests/) | `test_ttrpg`: writer / conventions / parser / golden tests. |

The shared scanner/token-stream ([tools/common/parse/](../common/parse/)) is
reused by the engine's DSL too — keep generator-only token kinds in `parser.h`,
not in the shared lib.

## Pipeline

1. **`Tokenize` / `TParser`** ([parser](include/ttrpg/parser.h)) — `Parse()`
   reads leading `import "...";` directives, then `enum` / `variant` / `class`
   declarations. Field parsing (`ParseField`) is shared between `class` bodies
   and `variant` alternative payloads.
2. **`LoadAll`** ([module](include/ttrpg/module.h)) — loads the primary file plus
   all transitive imports (relative paths, cycle-detected) and builds the
   symbol table `name -> TTypeInfo{ OwnerStem, ETypeKind }`. Duplicate type
   names across the import graph are an error.
3. **`EmitHeader` / `EmitImpl`** ([emit](include/ttrpg/emit.h)) — wrap the
   caller's `ostream` in a `TCppWriter` and walk the primary module's enums,
   then variants, then classes (order matters: classes may reference variants
   via `set<V>`, variants reference enums). Cross-file field types pull in the
   generated sibling header (`DeriveSiblingInclude`).

## Schema language

```ttrpg
import "other.ttrpg";          // makes other.ttrpg's types visible (recursive, cycle-checked)

enum EFoo { A, B, C }          // -> enum class EFoo + ToString / EFooFromString

variant TBar {                 // -> closed sum type (see below)
    Flag;                      //   flag alternative (no payload)
    WithDie  { EDieSize Die; } //   parameterized alternative (class-style fields, defaults allowed)
}

class TBaz {                   // -> data class (FromJson / GetAst / RegisterDslProperties)
    int Count = 0;             //   primitive field with default
    EFoo Kind;                 //   enum field
    TMaterial Mat;             //   class field (resolves via factory)
    set<EFoo> Flags;           //   set<Enum>    -> std::set<EFoo>
    set<TBar> Bars;            //   set<Variant> -> TVariantMap<EBarKind, TBar>
}
```

- **Type names are verbatim C++** (`TArmor`, `EArmorCategory`) — the generator
  never silently adds a `T`/`E` prefix.
- **Field types**: `int`, `bool`, `string`, the built-in value type
  `BoundedQuantity`, or any schema-declared enum / class / variant. Dispatch is
  by the symbol table (`EFieldKind`) — no `ref`/`owned` keyword. `EFieldKind`:
  `Primitive | Enum | Class | Variant | BoundedQuantity`.
- **`BoundedQuantity`** is a built-in scalar value type (a `{current_value,
  max_value}` pair). Like `int`/`string` it is recognized at the conventions
  layer (`IsBuiltinBoundedQuantity`), needs no lexer/parser change, and lowers to
  the hand-written `TBoundedQuantity` runtime type
  ([pf2e_engine/common/bounded_quantity.h](../../pf2e_engine/include/pf2e_engine/common/bounded_quantity.h))
  — the same "keyword → hand-written runtime type" arrangement as
  `set<Variant>` → `TVariantMap` (below). It loads via
  `TBoundedQuantity::FromJson` and is owned (recursive) in the AST. Scalar only —
  `set<BoundedQuantity>` is rejected.
- **`set<T>`**: `T` must be a schema-declared **enum** (lowers to `std::set`) or
  **variant** (lowers to `TVariantMap`). Primitive/class set elements are an
  error. Set fields cannot have a default (absent JSON key ⇒ empty).
- **Defaults**: int literals, `true`/`false`, an enum value identifier, or
  `max_int` / `min_int`. Class/variant fields have no defaults.
- **Computed defaults**: a field's `= <expr>` may be an arithmetic expression
  (`+ - * /`, parens, int literals) over **sibling fields** referenced by their
  PascalCase names, including **member access** into class-ref fields, e.g.
  `BoundedQuantity Hitpoints = Race.Hitpoints + Level * (Class.Hitpoints +
  Characteristic.Constitution);` (`Race.Hitpoints` lowers to the getter call
  `r.Race_.Hitpoints()`). Like all defaults it is **JSON-overridable**: if the
  JSON key is present the JSON value wins, otherwise the expression is
  evaluated. Only `int` and `BoundedQuantity` fields may be computed (a
  `BoundedQuantity` takes the int result as a full `{value, value}`); referenced
  fields (and member targets) must be `int`. The classification — constant
  default vs computed — is: a binary/member node, or a lone identifier naming a
  sibling field, is *computed*; a literal or a lone non-field identifier (enum
  value / `max_int`) is a *constant default* (unchanged `j.value` behavior,
  existing schemas untouched). Computed fields are a **class** feature only —
  variant alternative payloads reject non-constant initializers.
- **Expression front-end is shared.** Initializer expressions are not parsed by
  the schema parser: the schema lexer raw-captures everything after `=` up to
  `;` into an `InitExpr` token, and `expr::Parse` (the shared
  [tools/common/expr/](../common/expr/) library, also used by the engine's
  runtime DSL) turns it into an `expr::TExprNode`. The codegen lowers the
  supported subset (int literals, sibling-field vars, member access, arithmetic)
  to C++ and rejects the rest (calls, comparison/logical, `$`-vars) with a clear
  codegen error; the DSL evaluates the full grammar. This is why there is no
  arithmetic/member-access parser in `ttrpg` itself.

## Naming conventions (automatic, no per-field attributes)

A PascalCase field `FooBar` →

| Aspect | Result |
|---|---|
| C++ member (class) | `FooBar_` |
| C++ member (variant payload struct) | `FooBar` (public, no underscore) |
| Public getter (class) | `FooBar()` |
| JSON key | `foo_bar` (`PascalToSnake`) |
| DSL property | `$obj.foo_bar` |
| AST key | `foo_bar` |

Enum/variant JSON tokens are the **identifier verbatim** (PascalCase):
`EFooFromString("A")`, trait `"Finesse"`, die `"D8"`. (The original variant
task spec suggested lowercase `"fatal"`; we follow the existing enum
convention instead — verbatim PascalCase — so all generated `FromString`
casing is uniform.)

## What gets emitted

**Per enum**: `enum class EFoo`, `std::string ToString(EFoo)`,
`EFoo EFooFromString(const std::string&)`.

**Per class**: getters; `static T FromJson(const json&, const TGameObjectFactory&)`
(primitives `j.at(key).get<T>()` or `j.value(key, default)`; enums via
`FromString`; class fields via `factory.Create<T>(...Register(...))`; set
fields loop-insert); `TAstNode GetAst(TAstContext&)` (`AddOwnedObject` for
class/variant fields, `AddValueField` otherwise, container nodes for sets) plus
a `TIsAstRecursive<T>` specialization; `static void RegisterDslProperties()`
(only `int`/`bool` scalar fields; everything else emitted as a
`// dsl: '...' skipped` comment). Generated classes carry an unused
`ast_layout_sentinel_` but **no** `AST_ASSERT_LAYOUT` — the schema is the source
of truth, so the "edited fields without updating GetAst" failure mode is
structurally impossible.

**Per variant** `TBar` (kind enum name = `VariantKindEnum`: strip leading `T`,
prefix `E`, suffix `Kind` ⇒ `EBarKind`):
- the **kind enum** `EBarKind { ... }` + its `ToString`/`FromString`, emitted
  through the normal enum emitter so casing matches `enum` exactly;
- one **payload struct** per alternative (`TBar` + alt name ⇒ `TBarWithDie`),
  each with `static constexpr Kind` and its (defaulted) fields;
- the **wrapper class** `TBar` holding `using TPayload = std::variant<...>`, a
  default ctor, a converting ctor from any alternative (constrained with
  `requires (!is_same<decay_t<T>, TBar>)` so it never hijacks copy/move —
  required for `TVariantMap` to store it by value), `Kind()`, `Payload()`,
  `template<class T> const T* TryGet()`, plus `FromJson` / `GetAst` and a
  `TIsAstRecursive<TBar>` specialization. No `RegisterDslProperties` (variants
  aren't DSL-exposed).

### Variant JSON authoring forms (`FromJson`)
- **flag**: bare string — `"Finesse"`.
- **single-field**: single-key object, value parsed directly as the field type
  — `{"Fatal": "D8"}`, `{"Thrown": 20}`.
- **multi-field**: single-key object whose value is an object of named fields,
  with defaults applied for missing keys.

Malformed input throws `std::runtime_error`: flag-with-params,
param-without-params, multi-key object, unknown kind. Note the object form is
read with iterator `.key()`/`.value()` — nlohmann json iterators do **not**
structured-bind into `[key, val]`.

### `set<Variant>` lowering — `TVariantMap`
`set<TBar>` becomes `TVariantMap<EBarKind, TBar>`
([pf2e_engine/common/variant_map.h](../../pf2e_engine/include/pf2e_engine/common/variant_map.h),
hand-written, lives once). Keyed by `Kind()`: a weapon never has two traits of
the same kind, and rules code wants `Has(kind)` / `Get<TAlt>()` (typed,
nullptr-if-absent), not index iteration. `std::map` iteration order ⇒
deterministic AST. The same header provides the `overloaded` helper for
exhaustive `std::visit` over `Payload()`.

## TCppWriter (formatting)

The emitters never write `\n` or leading spaces. They describe *structure* and
*lines* through [`TCppWriter`](include/ttrpg/cpp_writer.h), which owns the indent
depth and brace placement and produces the final formatted text (there is **no**
external clang-format step — the writer is the single source of formatting).

- `Line` / `EmptyLine` / `Comment` / `Include` — leaf lines at the current indent.
- `Block(opener, closer, body)` — emits `opener`, runs `body` one level deeper,
  emits `closer`. `Class` / `Struct` / `Function` / `Switch` / `Case` are thin
  wrappers over it.
- `PublicSection` / `PrivateSection` — place the access label one level *out*
  from the members it introduces (so inside a `Class` body the label lands at
  the class's own indent).
- `Indented(body)` — body one level deeper with no braces; for continuation
  lines of a single statement (the `std::variant<...>` alternative list, a
  wrapped `requires`, the `std::visit` continuation).

To change generated formatting, change the writer — not the emitters. The golden
tests (below) lock the output, so any writer change shows up as a golden diff.

## Factory plumbing for class fields

When a class type is loaded by name from JSON (e.g. `TArmor` has
`TMaterial Material;` and armor JSON has `"material": "steel"`), the referenced
class must be registered with `TGameObjectFactory`: its own
`TFactoryStorage<T>`, a `Read<T>`, a branch in `GetFactoryStorage<T>`, and a
`kReaderMapping` entry. Read methods for classes that themselves have class
fields store **lazy** lambdas so refs resolve at `Create` time regardless of
load order (the long-standing `ReadCreature` pattern). Variants load eagerly
through their own `FromJson` and need no factory storage.

## Build integration

[pf2e_engine/src/inventory/CMakeLists.txt](../../pf2e_engine/src/inventory/CMakeLists.txt):
one `add_custom_command` per schema invoking `$<TARGET_FILE:ttrpg_codegen>`.
When schema A imports B, A's `DEPENDS` must list B's `.ttrpg` so editing B
regenerates A. The `pf2e_engine_generated_headers` target forces ordering so
consumers see headers before compiling; `CMP0118 NEW` (top-level) lets the
`GENERATED` source property cross directory scopes. The top-level CMake adds
`tools/ttrpg` before `pf2e_engine` so the `ttrpg_codegen` binary exists when
commands resolve.

## Tests

`test_ttrpg` ([tests/](tests/), run via `ctest`) links the `ttrpg` library:
- **test_writer** — `TCppWriter` scopes/indent/brace balance.
- **test_conventions** — `PascalToSnake`, `VariantKindEnum`, `FieldKindOf`,
  `CppMemberType` (set<enum> vs set<variant>), `DefaultExprToCpp`.
- **test_parser** — the three variant alternative forms, imports, and the
  duplicate-type / circular-import errors.
- **test_golden** — generates [tests/fixtures/](tests/fixtures/) `basic.ttrpg`
  and `variant.ttrpg` and byte-compares against committed `*.golden.h/.cpp`.
  These goldens are the human-readable reference for the writer's formatting;
  regenerate them (run `ttrpg_codegen` on the fixture with an out-h path under
  `.../include/`) whenever a deliberate output change lands.

The engine's own tests are the end-to-end gate: the generated `pf2e_engine`
sources must compile and all existing tests pass after any generator change.

## Extending the generator

- **New field on an existing type**: edit the `.ttrpg`, `make`. Getter/JSON
  key/DSL property/AST all follow. Wire up callers.
- **New generated type module**: add `<name>.ttrpg`, mirror the existing
  custom-command block in the inventory CMake (command + `target_sources` +
  `pf2e_engine_generated_headers` deps), and call
  `T<Class>::RegisterDslProperties()` from `RegisterAll()` in
  [builtins.cpp](../../pf2e_engine/src/dsl/builtins.cpp) if it's a class.
- **New scalar field shape** (e.g. another container, `optional`): add the
  token(s) in `Tokenize`, parse into `TFieldDecl` (`EContainer` or a new flag),
  then handle it in `CppMemberType`, `ScalarParseExpr`/`LoadFieldCall`,
  `EmitClassImpl`'s FromJson/GetAst branches, and the header include scan in
  `EmitHeader`. `ScalarParseExpr` is the single place that maps a json-value
  expression to a parse call for each `EFieldKind` — reuse it.

## Known limitations / out of scope

- DSL exposes only `int`/`bool` scalar fields (no `string`, enum, variant, or
  container) — widening needs new `TDslValue` alternatives.
- No inheritance, methods, or `list`/`optional`/`map` field shapes yet.
- Migrating `BaseDiceSize` (`int`) to `EDieSize` was left as-is (not done).
- Variants are **closed** by design: adding an alternative and regenerating
  makes every `std::visit` site built with `overloaded` fail to compile until
  the new case is handled. That compile-time exhaustiveness is intentional.
