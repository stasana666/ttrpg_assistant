// ttrpg_codegen: generates C++ headers/sources from .ttrpg schema files.
//
// Usage: ttrpg_codegen --schema <path> --out-h <path> --out-cpp <path>
//
// Schema conventions (single source of truth):
//   - `enum EFoo { A, B }` -> emits `enum class EFoo` + ToString / EFooFromString.
//   - `class TBar { int FieldName = 0; }` emits a class with:
//       - private member FieldName_ (PascalCase + trailing underscore)
//       - public getter FieldName() (PascalCase, no underscore)
//       - JSON load: r.FieldName_ = j.at("field_name").get<...>()  (snake_case key)
//       - AST: AddValueField(node, "field_name", FieldName_)
//       - DSL: TPropertyRegistry<TBar>::Instance().Register("field_name", ...)
//   - int/bool fields are auto-exposed to DSL; others are skipped with a comment.
//   - Class-typed fields (any type that resolves to a `class T { ... }` in
//     the schema, possibly through `import`) load via the factory and serialize
//     into the AST via AddOwnedObject. No special keyword needed -- the
//     symbol-table lookup makes the dispatch unambiguous.
//   - `import "other.ttrpg";` at the top of a file makes types declared in
//     "other.ttrpg" visible. Imports are resolved relative to the importing file.

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

// =================== Tokenizer ===================

enum class ETok {
    Ident,
    IntLiteral,
    StringLiteral,
    LBrace,
    RBrace,
    Semi,
    Comma,
    Equals,
    End,
};

struct TToken {
    ETok kind = ETok::End;
    std::string text;
    int line = 0;
    int col = 0;
};

class TLexer {
public:
    explicit TLexer(std::string src) : src_(std::move(src)) {}

    std::vector<TToken> Tokenize() {
        std::vector<TToken> out;
        while (pos_ < src_.size()) {
            SkipWsAndComments();
            if (pos_ >= src_.size()) {
                break;
            }
            int sl = line_, sc = col_;
            char c = src_[pos_];
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                std::string id;
                while (pos_ < src_.size() &&
                       (std::isalnum(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '_')) {
                    id += src_[pos_];
                    Advance();
                }
                out.push_back({ETok::Ident, id, sl, sc});
            } else if (std::isdigit(static_cast<unsigned char>(c)) ||
                       (c == '-' && pos_ + 1 < src_.size() &&
                        std::isdigit(static_cast<unsigned char>(src_[pos_ + 1])))) {
                std::string n;
                if (c == '-') {
                    n += '-';
                    Advance();
                }
                while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
                    n += src_[pos_];
                    Advance();
                }
                out.push_back({ETok::IntLiteral, n, sl, sc});
            } else if (c == '"') {
                Advance();
                std::string s;
                while (pos_ < src_.size() && src_[pos_] != '"') {
                    if (src_[pos_] == '\n') {
                        throw std::runtime_error(
                            "lexer: newline inside string literal at line " +
                            std::to_string(sl) + ", col " + std::to_string(sc));
                    }
                    s += src_[pos_];
                    Advance();
                }
                if (pos_ >= src_.size()) {
                    throw std::runtime_error(
                        "lexer: unterminated string literal starting at line " +
                        std::to_string(sl) + ", col " + std::to_string(sc));
                }
                Advance();
                out.push_back({ETok::StringLiteral, s, sl, sc});
            } else {
                switch (c) {
                    case '{': out.push_back({ETok::LBrace, "{", sl, sc}); Advance(); break;
                    case '}': out.push_back({ETok::RBrace, "}", sl, sc}); Advance(); break;
                    case ';': out.push_back({ETok::Semi, ";", sl, sc}); Advance(); break;
                    case ',': out.push_back({ETok::Comma, ",", sl, sc}); Advance(); break;
                    case '=': out.push_back({ETok::Equals, "=", sl, sc}); Advance(); break;
                    default:
                        throw std::runtime_error(
                            "lexer: unexpected character '" + std::string(1, c) +
                            "' at line " + std::to_string(sl) + ", col " + std::to_string(sc));
                }
            }
        }
        out.push_back({ETok::End, "", line_, col_});
        return out;
    }

private:
    void Advance() {
        if (pos_ < src_.size()) {
            if (src_[pos_] == '\n') {
                ++line_;
                col_ = 1;
            } else {
                ++col_;
            }
            ++pos_;
        }
    }

    void SkipWsAndComments() {
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (std::isspace(static_cast<unsigned char>(c))) {
                Advance();
            } else if (c == '/' && pos_ + 1 < src_.size() && src_[pos_ + 1] == '/') {
                while (pos_ < src_.size() && src_[pos_] != '\n') {
                    Advance();
                }
            } else {
                break;
            }
        }
    }

    std::string src_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
};

// =================== Schema AST ===================

struct TEnumDecl {
    std::string Name;
    std::vector<std::string> Values;
};

struct TFieldDecl {
    std::string TypeName;
    std::string Name;
    std::optional<std::string> DefaultExpr;
};

struct TClassDecl {
    std::string Name;
    std::vector<TFieldDecl> Fields;
};

struct TSchemaModule {
    std::vector<std::string> Imports;       // raw paths from import directives
    std::vector<TEnumDecl> Enums;
    std::vector<TClassDecl> Classes;
};

// =================== Parser ===================

class TParser {
public:
    explicit TParser(std::vector<TToken> toks) : toks_(std::move(toks)) {}

    TSchemaModule Parse() {
        TSchemaModule mod;
        while (Peek().kind == ETok::Ident && Peek().text == "import") {
            Advance();
            if (Peek().kind != ETok::StringLiteral) {
                Throw("expected string literal after 'import'");
            }
            mod.Imports.push_back(Peek().text);
            Advance();
            Expect(ETok::Semi);
        }
        while (Peek().kind != ETok::End) {
            const TToken& t = Peek();
            if (t.kind != ETok::Ident) {
                Throw("expected 'enum' or 'class'");
            }
            if (t.text == "enum") {
                mod.Enums.push_back(ParseEnum());
            } else if (t.text == "class") {
                mod.Classes.push_back(ParseClass());
            } else if (t.text == "import") {
                Throw("'import' must appear before any class/enum");
            } else {
                Throw("expected 'enum' or 'class'");
            }
        }
        return mod;
    }

private:
    TEnumDecl ParseEnum() {
        ExpectIdentText("enum");
        TEnumDecl e;
        e.Name = ExpectIdent("enum name");
        Expect(ETok::LBrace);
        while (Peek().kind != ETok::RBrace) {
            e.Values.push_back(ExpectIdent("enum value"));
            if (Peek().kind == ETok::Comma) {
                Advance();
            } else {
                break;
            }
        }
        Expect(ETok::RBrace);
        return e;
    }

    TClassDecl ParseClass() {
        ExpectIdentText("class");
        TClassDecl c;
        c.Name = ExpectIdent("class name");
        Expect(ETok::LBrace);
        while (Peek().kind != ETok::RBrace) {
            c.Fields.push_back(ParseField());
        }
        Expect(ETok::RBrace);
        return c;
    }

    TFieldDecl ParseField() {
        TFieldDecl f;
        f.TypeName = ExpectIdent("field type");
        f.Name = ExpectIdent("field name");
        if (Peek().kind == ETok::Equals) {
            Advance();
            f.DefaultExpr = ParseDefault();
        }
        Expect(ETok::Semi);
        return f;
    }

    std::string ParseDefault() {
        const TToken& t = Peek();
        if (t.kind == ETok::IntLiteral) {
            std::string s = t.text;
            Advance();
            return s;
        }
        if (t.kind == ETok::Ident) {
            std::string s = t.text;
            Advance();
            return s;
        }
        Throw("expected default value");
    }

    const TToken& Peek() const { return toks_[pos_]; }
    void Advance() { ++pos_; }

    void Expect(ETok k) {
        if (Peek().kind != k) {
            Throw("unexpected token");
        }
        Advance();
    }

    void ExpectIdentText(const std::string& text) {
        if (Peek().kind != ETok::Ident || Peek().text != text) {
            Throw("expected '" + text + "'");
        }
        Advance();
    }

    std::string ExpectIdent(const std::string& what) {
        if (Peek().kind != ETok::Ident) {
            Throw("expected " + what);
        }
        std::string s = Peek().text;
        Advance();
        return s;
    }

    [[noreturn]] void Throw(const std::string& msg) {
        const TToken& t = Peek();
        throw std::runtime_error(
            "parser: " + msg + " (got '" + t.text + "' at line " +
            std::to_string(t.line) + ", col " + std::to_string(t.col) + ")");
    }

    std::vector<TToken> toks_;
    size_t pos_ = 0;
};

// =================== Module loading & symbol table ===================

TSchemaModule ParseFile(const fs::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open schema: " + path.string());
    }
    std::stringstream buf;
    buf << in.rdbuf();
    TLexer lex(buf.str());
    auto toks = lex.Tokenize();
    TParser parser(std::move(toks));
    return parser.Parse();
}

struct TTypeInfo {
    std::string OwnerStem;   // filename stem of the .ttrpg that declares this type
    bool IsEnum = false;
};

struct TLoadedSchemas {
    TSchemaModule Primary;
    std::string PrimaryStem;
    // Stem -> module, for all transitively-loaded files (including primary).
    std::unordered_map<std::string, TSchemaModule> ByStem;
    // Type name -> (owner_stem, is_enum). Built across all loaded modules.
    std::unordered_map<std::string, TTypeInfo> SymbolTable;
};

void RegisterModuleSymbols(const std::string& stem,
                           const TSchemaModule& mod,
                           std::unordered_map<std::string, TTypeInfo>& table)
{
    for (const auto& e : mod.Enums) {
        auto it = table.find(e.Name);
        if (it != table.end()) {
            throw std::runtime_error(
                "duplicate type '" + e.Name + "' declared in '" + stem +
                ".ttrpg' (already declared in '" + it->second.OwnerStem + ".ttrpg')");
        }
        table.insert({e.Name, {stem, true}});
    }
    for (const auto& c : mod.Classes) {
        auto it = table.find(c.Name);
        if (it != table.end()) {
            throw std::runtime_error(
                "duplicate type '" + c.Name + "' declared in '" + stem +
                ".ttrpg' (already declared in '" + it->second.OwnerStem + ".ttrpg')");
        }
        table.insert({c.Name, {stem, false}});
    }
}

// Recursively load primary + all transitive imports.
// Detects cycles via the "loading" set.
TLoadedSchemas LoadAll(const fs::path& primaryPath) {
    TLoadedSchemas loaded;
    loaded.PrimaryStem = primaryPath.stem().string();

    std::unordered_set<std::string> loading;

    auto LoadRec = [&](auto& self, const fs::path& path) -> void {
        fs::path canon = fs::weakly_canonical(path);
        std::string canonStr = canon.string();
        std::string stem = canon.stem().string();

        if (loading.count(canonStr)) {
            throw std::runtime_error("circular import detected: " + canonStr);
        }
        if (loaded.ByStem.count(stem)) {
            return;  // already loaded
        }
        loading.insert(canonStr);

        TSchemaModule mod = ParseFile(canon);

        for (const std::string& imp : mod.Imports) {
            fs::path importPath = canon.parent_path() / imp;
            self(self, importPath);
        }

        RegisterModuleSymbols(stem, mod, loaded.SymbolTable);
        loaded.ByStem.emplace(stem, std::move(mod));
        loading.erase(canonStr);
    };

    LoadRec(LoadRec, primaryPath);
    loaded.Primary = loaded.ByStem.at(loaded.PrimaryStem);
    return loaded;
}

// =================== Conventions / helpers ===================

std::string PascalToSnake(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (std::isupper(static_cast<unsigned char>(c))) {
            if (i > 0) {
                out += '_';
            }
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else {
            out += c;
        }
    }
    return out;
}

bool IsBuiltinInt(const std::string& t) { return t == "int"; }
bool IsBuiltinBool(const std::string& t) { return t == "bool"; }
bool IsBuiltinString(const std::string& t) { return t == "string"; }
bool IsBuiltinPrimitive(const std::string& t) {
    return IsBuiltinInt(t) || IsBuiltinBool(t) || IsBuiltinString(t);
}

std::string CppTypeFor(const std::string& schemaType) {
    if (schemaType == "string") {
        return "std::string";
    }
    return schemaType;
}

// Per-field kind, derived from the (cross-file) symbol table.
enum class EFieldKind {
    Primitive,  // int / bool / string
    Enum,       // schema-declared enum
    Class,      // schema-declared class -> loads via factory, owned in AST
};

EFieldKind FieldKindOf(const TFieldDecl& f,
                       const std::unordered_map<std::string, TTypeInfo>& symbols)
{
    if (IsBuiltinPrimitive(f.TypeName)) {
        return EFieldKind::Primitive;
    }
    auto it = symbols.find(f.TypeName);
    if (it == symbols.end()) {
        // Caller (EmitHeader) will have already thrown on unknown class types;
        // primitives are filtered above, so reaching here means a typo that
        // somehow escaped. Treat as enum for graceful error pass-through.
        throw std::runtime_error("unknown type '" + f.TypeName + "'");
    }
    return it->second.IsEnum ? EFieldKind::Enum : EFieldKind::Class;
}

bool IsDslSupported(const TFieldDecl& f) {
    // Only primitives that fit into TDslValue. Enums and class refs are
    // never DSL-exposed by the generator (would require widening TDslValue).
    return IsBuiltinInt(f.TypeName) || IsBuiltinBool(f.TypeName);
}

std::string DefaultExprToCpp(const std::string& expr, const std::string& schemaType) {
    if (expr == "max_int") {
        return "std::numeric_limits<int>::max()";
    }
    if (expr == "min_int") {
        return "std::numeric_limits<int>::min()";
    }
    if (expr == "true" || expr == "false") {
        return expr;
    }
    if (!expr.empty() && (expr[0] == '-' || std::isdigit(static_cast<unsigned char>(expr[0])))) {
        return expr;
    }
    return schemaType + "::" + expr;
}

// Returns the C++ expression that loads `f` from JSON. Dispatches by field
// kind:
//   - primitive:  j.at("key").get<T>()
//   - enum:       EFooFromString(j.at("key").get<std::string>())
//   - class:      factory.Create<TFoo>(TGameObjectIdManager::Instance().Register(...))
std::string LoadFieldCall(const TFieldDecl& f,
                          EFieldKind kind,
                          const std::string& jsonKey)
{
    switch (kind) {
        case EFieldKind::Primitive:
            if (IsBuiltinInt(f.TypeName)) {
                return "j.at(\"" + jsonKey + "\").get<int>()";
            }
            if (IsBuiltinBool(f.TypeName)) {
                return "j.at(\"" + jsonKey + "\").get<bool>()";
            }
            return "j.at(\"" + jsonKey + "\").get<std::string>()";
        case EFieldKind::Enum:
            return f.TypeName + "FromString(j.at(\"" + jsonKey +
                   "\").get<std::string>())";
        case EFieldKind::Class:
            return "factory.Create<" + f.TypeName +
                   ">(TGameObjectIdManager::Instance().Register(j.at(\"" +
                   jsonKey + "\").get<std::string>()))";
    }
    throw std::runtime_error("unreachable: unknown EFieldKind");
}

// Derive a header include path of the form "pf2e_engine/<dir>/<stem>.h"
// from the primary --out-h argument and an arbitrary stem.
std::string DeriveSiblingInclude(const std::string& primaryOutH, const std::string& stem) {
    const std::string marker = "/include/";
    auto pos = primaryOutH.rfind(marker);
    std::string base;
    if (pos != std::string::npos) {
        base = primaryOutH.substr(pos + marker.size());
    } else {
        base = fs::path(primaryOutH).filename().string();
    }
    // Replace the basename in `base` with `<stem>.h`.
    fs::path basePath(base);
    return (basePath.parent_path() / (stem + ".h")).generic_string();
}

// =================== Emitters ===================

void EmitEnumDecl(std::ostream& os, const TEnumDecl& e) {
    os << "enum class " << e.Name << " {\n";
    for (const auto& v : e.Values) {
        os << "    " << v << ",\n";
    }
    os << "};\n\n";
    os << "std::string ToString(" << e.Name << " v);\n";
    os << e.Name << " " << e.Name << "FromString(const std::string& s);\n\n";
}

void EmitClassDecl(std::ostream& os, const TClassDecl& c) {
    os << "class " << c.Name << " {\n";
    os << "public:\n";
    for (const auto& f : c.Fields) {
        os << "    " << CppTypeFor(f.TypeName) << " " << f.Name
           << "() const { return " << f.Name << "_; }\n";
    }
    os << "\n";
    os << "    static " << c.Name
       << " FromJson(const nlohmann::json& j, const TGameObjectFactory& factory);\n";
    os << "    TAstNode GetAst(TAstContext& ctx) const;\n";
    os << "    static void RegisterDslProperties();\n";
    os << "\n";
    os << "private:\n";
    for (const auto& f : c.Fields) {
        std::string init = f.DefaultExpr
                               ? "{" + DefaultExprToCpp(*f.DefaultExpr, f.TypeName) + "}"
                               : "{}";
        os << "    " << CppTypeFor(f.TypeName) << " " << f.Name << "_" << init << ";\n";
    }
    os << "    [[maybe_unused]] char ast_layout_sentinel_[1] = {};\n";
    os << "};\n\n";
    os << "template <>\n";
    os << "struct TIsAstRecursive<" << c.Name << "> : std::true_type {};\n\n";
}

void EmitHeader(std::ostream& os,
                const TLoadedSchemas& loaded,
                const std::string& sourceName,
                const std::string& primaryOutH)
{
    const TSchemaModule& mod = loaded.Primary;

    os << "// AUTO-GENERATED FROM " << sourceName << " -- DO NOT EDIT.\n";
    os << "// Source of truth: pf2e_engine/data/schemas/" << sourceName << "\n";
    os << "#pragma once\n\n";
    os << "#include <pf2e_engine/common/ast/ast_constructable.h>\n";
    os << "\n";
    os << "#include <nlohmann/json_fwd.hpp>\n";
    os << "\n";
    os << "#include <limits>\n";
    os << "#include <string>\n\n";

    // Cross-file includes: any external type used as a field type (incl. refs)
    // pulls in the generated header that declares it.
    std::unordered_set<std::string> externalStems;
    for (const auto& c : mod.Classes) {
        for (const auto& f : c.Fields) {
            if (IsBuiltinPrimitive(f.TypeName)) continue;
            auto it = loaded.SymbolTable.find(f.TypeName);
            if (it == loaded.SymbolTable.end()) {
                throw std::runtime_error(
                    "unknown type '" + f.TypeName + "' referenced in class '" +
                    c.Name + "' (declare it in this file or import another .ttrpg)");
            }
            if (it->second.OwnerStem != loaded.PrimaryStem) {
                externalStems.insert(it->second.OwnerStem);
            }
        }
    }
    if (!externalStems.empty()) {
        for (const auto& stem : externalStems) {
            os << "#include <" << DeriveSiblingInclude(primaryOutH, stem) << ">\n";
        }
        os << "\n";
    }

    // Forward decl needed by FromJson signature
    os << "class TGameObjectFactory;\n\n";

    for (const auto& e : mod.Enums) {
        EmitEnumDecl(os, e);
    }
    for (const auto& c : mod.Classes) {
        EmitClassDecl(os, c);
    }
}

void EmitEnumImpl(std::ostream& os, const TEnumDecl& e) {
    os << "std::string ToString(" << e.Name << " v) {\n";
    os << "    switch (v) {\n";
    for (const auto& v : e.Values) {
        os << "        case " << e.Name << "::" << v << ": return \"" << v << "\";\n";
    }
    os << "    }\n";
    os << "    throw std::runtime_error(\"invalid " << e.Name << " value\");\n";
    os << "}\n\n";
    os << e.Name << " " << e.Name << "FromString(const std::string& s) {\n";
    for (const auto& v : e.Values) {
        os << "    if (s == \"" << v << "\") {\n";
        os << "        return " << e.Name << "::" << v << ";\n";
        os << "    }\n";
    }
    os << "    throw std::runtime_error(\"unknown " << e.Name << ": \\\"\" + s + \"\\\"\");\n";
    os << "}\n\n";
}

void EmitClassImpl(std::ostream& os,
                   const TClassDecl& c,
                   const std::unordered_map<std::string, TTypeInfo>& symbols)
{
    // Pre-classify each field once.
    std::vector<EFieldKind> kinds;
    kinds.reserve(c.Fields.size());
    bool usesFactory = false;
    for (const auto& f : c.Fields) {
        EFieldKind k = FieldKindOf(f, symbols);
        kinds.push_back(k);
        if (k == EFieldKind::Class) {
            usesFactory = true;
        }
    }

    os << c.Name << " " << c.Name
       << "::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory) {\n";
    if (!usesFactory) {
        os << "    (void)factory;\n";
    }
    os << "    " << c.Name << " r;\n";
    for (size_t i = 0; i < c.Fields.size(); ++i) {
        const auto& f = c.Fields[i];
        std::string key = PascalToSnake(f.Name);
        os << "    r." << f.Name << "_ = " << LoadFieldCall(f, kinds[i], key) << ";\n";
    }
    os << "    return r;\n";
    os << "}\n\n";

    os << "TAstNode " << c.Name << "::GetAst([[maybe_unused]] TAstContext& ctx) const {\n";
    os << "    TAstNode node = TAstNode::MakeObject(\"" << c.Name << "\");\n";
    for (size_t i = 0; i < c.Fields.size(); ++i) {
        const auto& f = c.Fields[i];
        std::string key = PascalToSnake(f.Name);
        if (kinds[i] == EFieldKind::Class) {
            // Generated class types are TIsAstRecursive::true_type, so use
            // AddOwnedObject which recurses; AddValueField would static_assert.
            os << "    AddOwnedObject(node, \"" << key << "\", " << f.Name << "_, ctx);\n";
        } else {
            os << "    AddValueField(node, \"" << key << "\", " << f.Name << "_);\n";
        }
    }
    os << "    return node;\n";
    os << "}\n\n";

    os << "void " << c.Name << "::RegisterDslProperties() {\n";
    os << "    auto& r = TPropertyRegistry<" << c.Name << ">::Instance();\n";
    for (size_t i = 0; i < c.Fields.size(); ++i) {
        const auto& f = c.Fields[i];
        std::string key = PascalToSnake(f.Name);
        if (IsDslSupported(f)) {
            os << "    r.Register(\"" << key << "\", [](const " << c.Name
               << "* obj, TEvalContext&) {\n";
            os << "        return TDslValue(obj->" << f.Name << "());\n";
            os << "    });\n";
        } else {
            const char* reasonPrefix = "unsupported type";
            switch (kinds[i]) {
                case EFieldKind::Class: reasonPrefix = "class field"; break;
                case EFieldKind::Enum:  reasonPrefix = "enum field"; break;
                case EFieldKind::Primitive: reasonPrefix = "unsupported primitive"; break;
            }
            os << "    // dsl: '" << key << "' skipped -- " << reasonPrefix
               << " '" << f.TypeName << "'\n";
        }
    }
    // r might be unused if the class has no DSL-eligible fields.
    bool anyDsl = false;
    for (const auto& f : c.Fields) {
        if (IsDslSupported(f)) { anyDsl = true; break; }
    }
    if (!anyDsl) {
        os << "    (void)r;\n";
    }
    os << "}\n\n";
}

void EmitImpl(std::ostream& os,
              const TLoadedSchemas& loaded,
              const std::string& sourceName,
              const std::string& headerInclude) {
    const TSchemaModule& mod = loaded.Primary;

    os << "// AUTO-GENERATED FROM " << sourceName << " -- DO NOT EDIT.\n";
    os << "#include <" << headerInclude << ">\n\n";
    os << "#include <pf2e_engine/common/ast/ast_helpers.h>\n";
    os << "#include <pf2e_engine/dsl/property_registry.h>\n";
    os << "#include <pf2e_engine/dsl/value.h>\n";
    os << "#include <pf2e_engine/game_object_logic/game_object_factory.h>\n";
    os << "#include <pf2e_engine/game_object_logic/game_object_id.h>\n";
    os << "\n";
    os << "#include <nlohmann/json.hpp>\n";
    os << "\n";
    os << "#include <stdexcept>\n";
    os << "#include <string>\n\n";
    for (const auto& e : mod.Enums) {
        EmitEnumImpl(os, e);
    }
    for (const auto& c : mod.Classes) {
        EmitClassImpl(os, c, loaded.SymbolTable);
    }
}

// =================== CLI ===================

struct TArgs {
    std::string SchemaPath;
    std::string OutH;
    std::string OutCpp;
};

TArgs ParseArgs(int argc, char** argv) {
    TArgs a;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto NextVal = [&]() -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("missing value for " + arg);
            }
            return argv[++i];
        };
        if (arg == "--schema") {
            a.SchemaPath = NextVal();
        } else if (arg == "--out-h") {
            a.OutH = NextVal();
        } else if (arg == "--out-cpp") {
            a.OutCpp = NextVal();
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }
    if (a.SchemaPath.empty() || a.OutH.empty() || a.OutCpp.empty()) {
        throw std::runtime_error(
            "usage: ttrpg_codegen --schema <path> --out-h <path> --out-cpp <path>");
    }
    return a;
}

// Compute the #include path for the primary header.
// Out-h is .../include/<rest>; return <rest>.
std::string DerivePrimaryHeaderInclude(const std::string& outH) {
    const std::string marker = "/include/";
    auto pos = outH.rfind(marker);
    if (pos != std::string::npos) {
        return outH.substr(pos + marker.size());
    }
    return fs::path(outH).filename().string();
}

int main(int argc, char** argv) {
    try {
        TArgs args = ParseArgs(argc, argv);

        TLoadedSchemas loaded = LoadAll(args.SchemaPath);

        std::string sourceName = fs::path(args.SchemaPath).filename().string();
        std::string headerInclude = DerivePrimaryHeaderInclude(args.OutH);

        fs::create_directories(fs::path(args.OutH).parent_path());
        fs::create_directories(fs::path(args.OutCpp).parent_path());

        {
            std::ofstream out(args.OutH);
            if (!out) {
                throw std::runtime_error("cannot write: " + args.OutH);
            }
            EmitHeader(out, loaded, sourceName, args.OutH);
        }
        {
            std::ofstream out(args.OutCpp);
            if (!out) {
                throw std::runtime_error("cannot write: " + args.OutCpp);
            }
            EmitImpl(out, loaded, sourceName, headerInclude);
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ttrpg_codegen: " << e.what() << "\n";
        return 1;
    }
}
