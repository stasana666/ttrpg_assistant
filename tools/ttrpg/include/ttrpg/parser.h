#pragma once

// Tokenizer + recursive-descent parser: `.ttrpg` source text -> TSchemaModule.

#include <ttrpg/schema_ast.h>

#include <parse/scanner.h>
#include <parse/token_stream.h>

#include <string>
#include <utility>
#include <unordered_set>
#include <vector>

enum class ETok {
    Ident,
    IntLiteral,
    StringLiteral,
    LBrace,
    RBrace,
    LAngle,
    RAngle,
    Semi,
    Comma,
    Equals,
    End,
};

using TToken = parse::TToken<ETok>;

std::vector<TToken> Tokenize(std::string src);

class TParser {
public:
    explicit TParser(std::vector<TToken> toks) : ts_(std::move(toks)) {}

    TSchemaModule Parse() {
        TSchemaModule mod;
        while (ts_.Peek().kind == ETok::Ident && ts_.Peek().text == "import") {
            ts_.Advance();
            if (ts_.Peek().kind != ETok::StringLiteral) {
                ts_.Throw("expected string literal after 'import'");
            }
            mod.Imports.push_back(ts_.Peek().text);
            ts_.Advance();
            ts_.Expect(ETok::Semi, "';'");
        }
        while (ts_.Peek().kind != ETok::End) {
            const TToken& t = ts_.Peek();
            if (t.kind != ETok::Ident) {
                ts_.Throw("expected 'enum' or 'class'");
            }
            if (t.text == "enum") {
                mod.Enums.push_back(ParseEnum());
            } else if (t.text == "variant") {
                mod.Variants.push_back(ParseVariant());
            } else if (t.text == "class") {
                mod.Classes.push_back(ParseClass());
            } else if (t.text == "import") {
                ts_.Throw("'import' must appear before any class/enum/variant");
            } else {
                ts_.Throw("expected 'enum', 'variant', or 'class'");
            }
        }
        return mod;
    }

private:
    TEnumDecl ParseEnum() {
        ExpectIdentText("enum");
        TEnumDecl e;
        e.Name = ExpectIdent("enum name");
        ts_.Expect(ETok::LBrace, "'{'");
        while (ts_.Peek().kind != ETok::RBrace) {
            e.Values.push_back(ExpectIdent("enum value"));
            if (ts_.Peek().kind == ETok::Comma) {
                ts_.Advance();
            } else {
                break;
            }
        }
        ts_.Expect(ETok::RBrace, "'}'");
        return e;
    }

    TClassDecl ParseClass() {
        ExpectIdentText("class");
        TClassDecl c;
        c.Name = ExpectIdent("class name");
        ts_.Expect(ETok::LBrace, "'{'");
        while (ts_.Peek().kind != ETok::RBrace) {
            c.Fields.push_back(ParseField());
        }
        ts_.Expect(ETok::RBrace, "'}'");
        return c;
    }

    TVariantDecl ParseVariant() {
        ExpectIdentText("variant");
        TVariantDecl v;
        v.Name = ExpectIdent("variant name");
        ts_.Expect(ETok::LBrace, "'{'");
        std::unordered_set<std::string> altNames;
        while (ts_.Peek().kind != ETok::RBrace) {
            TVariantAlt alt;
            alt.Name = ExpectIdent("alternative name");
            if (!altNames.insert(alt.Name).second) {
                ts_.Throw("duplicate alternative '" + alt.Name + "' in variant '" + v.Name + "'");
            }
            if (ts_.Peek().kind == ETok::Semi) {
                // Flag alternative: no payload.
                ts_.Advance();
            } else if (ts_.Peek().kind == ETok::LBrace) {
                // Parameterized alternative: a brace block of class-style fields.
                ts_.Advance();
                std::unordered_set<std::string> fieldNames;
                while (ts_.Peek().kind != ETok::RBrace) {
                    TFieldDecl f = ParseField();
                    if (!fieldNames.insert(f.Name).second) {
                        ts_.Throw("duplicate field '" + f.Name + "' in alternative '" +
                                  alt.Name + "'");
                    }
                    alt.Fields.push_back(std::move(f));
                }
                ts_.Expect(ETok::RBrace, "'}'");
            } else {
                ts_.Throw("expected ';' or '{' after alternative name '" + alt.Name + "'");
            }
            v.Alternatives.push_back(std::move(alt));
        }
        ts_.Expect(ETok::RBrace, "'}'");
        return v;
    }

    TFieldDecl ParseField() {
        TFieldDecl f;
        std::string head = ExpectIdent("field type");
        if (head == "set") {
            f.Container = EContainer::Set;
            ts_.Expect(ETok::LAngle, "'<' after 'set'");
            f.TypeName = ExpectIdent("set element type");
            ts_.Expect(ETok::RAngle, "'>'");
        } else {
            f.TypeName = head;
        }
        f.Name = ExpectIdent("field name");
        if (ts_.Peek().kind == ETok::Equals) {
            if (f.Container != EContainer::None) {
                ts_.Throw("container fields cannot have a default value");
            }
            ts_.Advance();
            f.DefaultExpr = ParseDefault();
        }
        ts_.Expect(ETok::Semi, "';'");
        return f;
    }

    std::string ParseDefault() {
        const TToken& t = ts_.Peek();
        if (t.kind == ETok::IntLiteral || t.kind == ETok::Ident) {
            std::string s = t.text;
            ts_.Advance();
            return s;
        }
        ts_.Throw("expected default value");
    }

    void ExpectIdentText(const std::string& text) {
        if (ts_.Peek().kind != ETok::Ident || ts_.Peek().text != text) {
            ts_.Throw("expected '" + text + "'");
        }
        ts_.Advance();
    }

    std::string ExpectIdent(const std::string& what) {
        if (ts_.Peek().kind != ETok::Ident) {
            ts_.Throw("expected " + what);
        }
        std::string s = ts_.Peek().text;
        ts_.Advance();
        return s;
    }

    parse::TTokenStream<ETok> ts_;
};
