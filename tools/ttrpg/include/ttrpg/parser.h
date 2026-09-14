#pragma once


#include <ttrpg/schema_ast.h>

#include <expr/parser.h>

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
    InitExpr,
    LParen,
    RParen,
    LBracket,
    RBracket,
    Operator,
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
            } else if (t.text == "class" || t.text == "GameEntity") {
                mod.Classes.push_back(ParseClass());
            } else if (t.text == "action" || t.text == "reaction" || t.text == "feature") {
                mod.Abilities.push_back(ParseAbility());
            } else if (t.text == "import") {
                ts_.Throw("'import' must appear before any class/enum/variant");
            } else {
                mod.Instances.push_back(ParseInstance());
            }
        }
        return mod;
    }

private:
    TGrantDecl ParseGrant() {
        TGrantDecl grant;
        grant.Location = ts_.Peek().loc;
        ExpectIdentText("grant");
        grant.Source = ReadExpression(ETok::Semi);
        ts_.Expect(ETok::Semi, "';'");
        return grant;
    }

    TAbilityDecl ParseAbility() {
        TAbilityDecl declaration;
        declaration.Location = ts_.Peek().loc;
        const std::string keyword = ts_.Consume().text;
        declaration.Kind = keyword == "action" ? EAbilityDeclarationKind::Action :
            keyword == "reaction" ? EAbilityDeclarationKind::Reaction : EAbilityDeclarationKind::Feature;
        declaration.Name = ExpectIdent("ability name");
        if (ts_.Peek().text == "for") {
            if (declaration.Kind != EAbilityDeclarationKind::Action) {
                ts_.Throw("compiled bodies currently support action declarations only");
            }
            ts_.Advance();
            declaration.OwnerType = ExpectIdent("action owner type");
            declaration.Body = ParseBody();
            ts_.Match(ETok::Semi);
        } else if (declaration.Kind == EAbilityDeclarationKind::Feature) {
            ts_.Expect(ETok::LBrace, "'{' after feature name");
            while (ts_.Peek().kind != ETok::RBrace) {
                declaration.Grants.push_back(ParseGrant());
            }
            ts_.Expect(ETok::RBrace, "'}'");
            ts_.Match(ETok::Semi);
        } else {
            ts_.Expect(ETok::Semi, "';' after action or reaction declaration");
        }
        return declaration;
    }

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
        TClassDecl c;
        c.GameEntity = ts_.Consume().text == "GameEntity";
        c.Name = ExpectIdent("class name");
        ts_.Expect(ETok::LBrace, "'{'");
        while (ts_.Peek().kind != ETok::RBrace) {
            if (ts_.Peek().text == "method" || ts_.Peek().text == "query") {
                c.Methods.push_back(ParseMethod());
            } else if (ts_.Peek().text == "grant") {
                c.Grants.push_back(ParseGrant());
            } else {
                c.Fields.push_back(ParseField());
            }
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
                ts_.Advance();
            } else if (ts_.Peek().kind == ETok::LBrace) {
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
        if (head == "derive") {
            f.Derived = true;
            head = ExpectIdent("field type after 'derive'");
        }
        if (head == "set") {
            if (f.Derived) {
                ts_.Throw("'derive' cannot be applied to a 'set' field");
            }
            f.Container = EContainer::Set;
            ts_.Expect(ETok::LAngle, "'<' after 'set'");
            f.TypeName = ExpectIdent("set element type");
            ts_.Expect(ETok::RAngle, "'>'");
        } else if (head == "collection") {
            if (f.Derived) {
                ts_.Throw("'derive' cannot be applied to a 'collection' field");
            }
            f.Container = EContainer::Collection;
            ts_.Expect(ETok::LAngle, "'<' after 'collection'");
            f.TypeName = ExpectIdent("collection element type");
            ts_.Expect(ETok::RAngle, "'>'");
        } else if (head == "map") {
            if (f.Derived) {
                ts_.Throw("'derive' cannot be applied to a 'map' field");
            }
            f.Container = EContainer::Map;
            ts_.Expect(ETok::LAngle, "'<' after 'map'");
            f.TypeName = ExpectIdent("map key type");
            ts_.Expect(ETok::Comma, "',' after map key type");
            f.ValueTypeName = ExpectIdent("map value type");
            ts_.Expect(ETok::RAngle, "'>'");
        } else {
            f.TypeName = head;
        }
        f.Name = ExpectIdent("field name");
        if (ts_.Peek().kind == ETok::Equals) {
            ts_.Advance();
            f.Init = ReadExpression(ETok::Semi);
        }
        if (f.Derived && !f.Init) {
            ts_.Throw("derived field '" + f.Name + "' requires an initializer expression");
        }
        ts_.Expect(ETok::Semi, "';'");
        return f;
    }

    expr::TExprNode ReadExpression(ETok end, bool assignment = false, const std::string& prefix = "", bool aggregate = false) {
        std::string source = prefix;
        int braces = 0;
        int parens = 0;
        int brackets = 0;
        auto cursor = ts_.Peek().loc;
        while (ts_.Peek().kind != ETok::End) {
            const auto& token = ts_.Peek();
            if (braces == 0 && parens == 0 && brackets == 0 &&
                (token.kind == end ||
                 (assignment && (token.text == "=" || token.text == "+=" || token.text == "-=")))) {
                break;
            }
            if (token.kind == ETok::RBrace && braces == 0) {
                ts_.Throw("expected expression terminator");
            }
            if (token.kind == ETok::LBrace) ++braces;
            if (token.kind == ETok::RBrace) --braces;
            if (token.kind == ETok::LParen) ++parens;
            if (token.kind == ETok::RParen) --parens;
            if (token.kind == ETok::LBracket) ++brackets;
            if (token.kind == ETok::RBracket) --brackets;
            while (cursor.line < token.loc.line) {
                source += '\n';
                ++cursor.line;
                cursor.col = 1;
            }
            while (cursor.col < token.loc.col) {
                source += ' ';
                ++cursor.col;
            }
            const auto text = token.kind == ETok::StringLiteral ? "\"" + token.text + "\"" : token.text;
            source += text;
            cursor.col += static_cast<int>(text.size());
            ts_.Advance();
            if (aggregate && braces == 0 && parens == 0 && brackets == 0) break;
        }
        if (source.empty()) ts_.Throw("expected expression");
        return expr::Parse(source);
    }

    TInstanceDecl ParseInstance() {
        TInstanceDecl instance;
        instance.TypeName = ExpectIdent("instance type");
        instance.Name = ExpectIdent("instance name");
        if (ts_.Peek().kind != ETok::LBrace) ts_.Throw("expected '{' after instance name");
        auto body = ReadExpression(ETok::Semi, false, instance.TypeName, true);
        instance.Value = std::move(body);
        ts_.Match(ETok::Semi);
        return instance;
    }

    TClassDecl::TMethod ParseMethod() {
        TClassDecl::TMethod method;
        method.Location = ts_.Peek().loc;
        method.Query = ts_.Consume().text == "query";
        method.ReturnType = ExpectIdent("method return type");
        method.Name = ExpectIdent("method name");
        ts_.Expect(ETok::LParen, "'('");
        if (ts_.Peek().kind != ETok::RParen) {
            do {
                TClassDecl::TParameter parameter;
                parameter.TypeName = ExpectIdent("parameter type");
                parameter.Name = ExpectIdent("parameter name");
                method.Parameters.push_back(std::move(parameter));
            } while (ts_.Match(ETok::Comma));
        }
        ts_.Expect(ETok::RParen, "')'");
        method.Body = ParseBody();
        return method;
    }

    std::vector<TClassDecl::TStatement> ParseBody() {
        ts_.Expect(ETok::LBrace, "'{'");
        std::vector<TClassDecl::TStatement> body;
        while (ts_.Peek().kind != ETok::RBrace) {
            TClassDecl::TStatement statement;
            statement.Location = ts_.Peek().loc;
            using K = TClassDecl::TStatement::EKind;
            if (ts_.Peek().text == "cost") {
                ts_.Advance();
                statement.Kind = K::Cost;
                statement.Target = ReadExpression(ETok::Equals);
                ts_.Expect(ETok::Equals, "'=' after resource cost field");
                statement.Value = ReadExpression(ETok::Semi);
            } else if (ts_.Peek().text == "choose") {
                ts_.Advance();
                statement.Kind = K::Choose;
                statement.TypeName = ExpectIdent("chosen entity type");
                statement.Name = ExpectIdent("chosen entity name");
                if (ts_.Peek().text == "where") {
                    ts_.Advance();
                    statement.Value = ReadExpression(ETok::Semi);
                }
            } else if (ts_.Peek().text == "let") {
                ts_.Advance();
                statement.Kind = K::Let;
                statement.Name = ExpectIdent("local name");
                ts_.Expect(ETok::Equals, "'='");
                statement.Value = ReadExpression(ETok::Semi);
            } else if (ts_.Peek().text == "require") {
                ts_.Advance();
                statement.Kind = K::Require;
                statement.Value = ReadExpression(ETok::Semi);
            } else if (ts_.Peek().text == "return") {
                ts_.Advance();
                statement.Kind = K::Return;
                if (ts_.Peek().kind != ETok::Semi) statement.Value = ReadExpression(ETok::Semi);
            } else if (ts_.Peek().text == "if") {
                ts_.Advance();
                statement.Kind = K::If;
                ts_.Expect(ETok::LParen, "'(' after if");
                statement.Value = ReadExpression(ETok::RParen);
                ts_.Expect(ETok::RParen, "')'");
                statement.Body = ParseBody();
                if (ts_.Peek().text == "else") {
                    ts_.Advance();
                    statement.ElseBody = ParseBody();
                }
                body.push_back(std::move(statement));
                continue;
            } else {
                auto expression = ReadExpression(ETok::Semi, true);
                const auto op = ts_.Peek().text;
                if (op == "=" || op == "+=" || op == "-=") {
                    ts_.Advance();
                    statement.Kind = op == "=" ? K::Assign : (op == "+=" ? K::AddAssign : K::SubAssign);
                    statement.Target = std::move(expression);
                    statement.Value = ReadExpression(ETok::Semi);
                } else {
                    statement.Kind = K::Call;
                    statement.Value = std::move(expression);
                }
            }
            ts_.Expect(ETok::Semi, "';'");
            body.push_back(std::move(statement));
        }
        ts_.Expect(ETok::RBrace, "'}'");
        return body;
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
