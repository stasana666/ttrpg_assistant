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

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// =================== Tokenizer ===================

enum class ETok {
    Ident,
    IntLiteral,
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
    std::vector<TEnumDecl> Enums;
    std::vector<TClassDecl> Classes;
};

// =================== Parser ===================

class TParser {
public:
    explicit TParser(std::vector<TToken> toks) : toks_(std::move(toks)) {}

    TSchemaModule Parse() {
        TSchemaModule mod;
        while (Peek().kind != ETok::End) {
            const TToken& t = Peek();
            if (t.kind != ETok::Ident) {
                Throw("expected 'enum' or 'class'");
            }
            if (t.text == "enum") {
                mod.Enums.push_back(ParseEnum());
            } else if (t.text == "class") {
                mod.Classes.push_back(ParseClass());
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

bool IsDslSupported(const std::string& schemaType) {
    return IsBuiltinInt(schemaType) || IsBuiltinBool(schemaType);
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

std::string JsonReadCall(const std::string& schemaType, const std::string& jsonKey) {
    if (IsBuiltinInt(schemaType)) {
        return "j.at(\"" + jsonKey + "\").get<int>()";
    }
    if (IsBuiltinBool(schemaType)) {
        return "j.at(\"" + jsonKey + "\").get<bool>()";
    }
    if (IsBuiltinString(schemaType)) {
        return "j.at(\"" + jsonKey + "\").get<std::string>()";
    }
    return schemaType + "FromString(j.at(\"" + jsonKey + "\").get<std::string>())";
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
    os << "    static " << c.Name << " FromJson(const nlohmann::json& j);\n";
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

void EmitHeader(std::ostream& os, const TSchemaModule& mod, const std::string& sourceName) {
    os << "// AUTO-GENERATED FROM " << sourceName << " -- DO NOT EDIT.\n";
    os << "// Source of truth: pf2e_engine/data/schemas/" << sourceName << "\n";
    os << "#pragma once\n\n";
    os << "#include <pf2e_engine/common/ast/ast_constructable.h>\n";
    os << "\n";
    os << "#include <nlohmann/json_fwd.hpp>\n";
    os << "\n";
    os << "#include <limits>\n";
    os << "#include <string>\n\n";
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

void EmitClassImpl(std::ostream& os, const TClassDecl& c) {
    os << c.Name << " " << c.Name << "::FromJson(const nlohmann::json& j) {\n";
    os << "    " << c.Name << " r;\n";
    for (const auto& f : c.Fields) {
        std::string key = PascalToSnake(f.Name);
        os << "    r." << f.Name << "_ = " << JsonReadCall(f.TypeName, key) << ";\n";
    }
    os << "    return r;\n";
    os << "}\n\n";

    os << "TAstNode " << c.Name << "::GetAst([[maybe_unused]] TAstContext& ctx) const {\n";
    os << "    TAstNode node = TAstNode::MakeObject(\"" << c.Name << "\");\n";
    for (const auto& f : c.Fields) {
        std::string key = PascalToSnake(f.Name);
        os << "    AddValueField(node, \"" << key << "\", " << f.Name << "_);\n";
    }
    os << "    return node;\n";
    os << "}\n\n";

    os << "void " << c.Name << "::RegisterDslProperties() {\n";
    os << "    auto& r = TPropertyRegistry<" << c.Name << ">::Instance();\n";
    for (const auto& f : c.Fields) {
        std::string key = PascalToSnake(f.Name);
        if (IsDslSupported(f.TypeName)) {
            os << "    r.Register(\"" << key << "\", [](const " << c.Name
               << "* obj, TEvalContext&) {\n";
            os << "        return TDslValue(obj->" << f.Name << "());\n";
            os << "    });\n";
        } else {
            os << "    // dsl: '" << key << "' skipped -- unsupported type '"
               << f.TypeName << "'\n";
        }
    }
    os << "}\n\n";
}

void EmitImpl(std::ostream& os,
              const TSchemaModule& mod,
              const std::string& sourceName,
              const std::string& headerInclude) {
    os << "// AUTO-GENERATED FROM " << sourceName << " -- DO NOT EDIT.\n";
    os << "#include <" << headerInclude << ">\n\n";
    os << "#include <pf2e_engine/common/ast/ast_helpers.h>\n";
    os << "#include <pf2e_engine/dsl/property_registry.h>\n";
    os << "#include <pf2e_engine/dsl/value.h>\n";
    os << "\n";
    os << "#include <nlohmann/json.hpp>\n";
    os << "\n";
    os << "#include <stdexcept>\n";
    os << "#include <string>\n\n";
    for (const auto& e : mod.Enums) {
        EmitEnumImpl(os, e);
    }
    for (const auto& c : mod.Classes) {
        EmitClassImpl(os, c);
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

// Compute the #include path. Out-h is .../include/<rest>; we return <rest>.
std::string DeriveHeaderInclude(const std::string& outH) {
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

        std::ifstream in(args.SchemaPath);
        if (!in) {
            throw std::runtime_error("cannot open schema: " + args.SchemaPath);
        }
        std::stringstream buf;
        buf << in.rdbuf();

        TLexer lex(buf.str());
        auto toks = lex.Tokenize();
        TParser parser(std::move(toks));
        TSchemaModule mod = parser.Parse();

        std::string sourceName = fs::path(args.SchemaPath).filename().string();
        std::string headerInclude = DeriveHeaderInclude(args.OutH);

        fs::create_directories(fs::path(args.OutH).parent_path());
        fs::create_directories(fs::path(args.OutCpp).parent_path());

        {
            std::ofstream out(args.OutH);
            if (!out) {
                throw std::runtime_error("cannot write: " + args.OutH);
            }
            EmitHeader(out, mod, sourceName);
        }
        {
            std::ofstream out(args.OutCpp);
            if (!out) {
                throw std::runtime_error("cannot write: " + args.OutCpp);
            }
            EmitImpl(out, mod, sourceName, headerInclude);
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ttrpg_codegen: " << e.what() << "\n";
        return 1;
    }
}
