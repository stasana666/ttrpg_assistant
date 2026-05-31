#include <pf2e_engine/dsl/lexer.h>

#include <parse/scanner.h>

#include <string>

std::vector<TToken> Tokenize(std::string_view src)
{
    parse::TScanner s{std::string(src)};
    std::vector<TToken> out;

    while (true) {
        s.SkipWhitespace();
        if (s.Eof()) {
            break;
        }
        parse::TSourceLocation loc = s.Loc();
        char c = s.Peek();

        if (parse::IsDigit(c)) {
            out.push_back({ETokenType::Number, s.ScanInteger(), loc});
            continue;
        }
        if (parse::IsIdentStart(c)) {
            out.push_back({ETokenType::Identifier, s.ScanIdent(), loc});
            continue;
        }

        // Two-char operators must be tried before their one-char prefixes.
        if (s.Try(">=")) { out.push_back({ETokenType::Ge, ">=", loc}); continue; }
        if (s.Try("<=")) { out.push_back({ETokenType::Le, "<=", loc}); continue; }
        if (s.Try("==")) { out.push_back({ETokenType::Eq, "==", loc}); continue; }
        if (s.Try("!=")) { out.push_back({ETokenType::Ne, "!=", loc}); continue; }
        if (s.Try("&&")) { out.push_back({ETokenType::And, "&&", loc}); continue; }
        if (s.Try("||")) { out.push_back({ETokenType::Or, "||", loc}); continue; }

        // Single-char tokens.
        switch (c) {
            case '$': s.Advance(); out.push_back({ETokenType::Dollar, "$", loc}); continue;
            case '.': s.Advance(); out.push_back({ETokenType::Dot,    ".", loc}); continue;
            case ',': s.Advance(); out.push_back({ETokenType::Comma,  ",", loc}); continue;
            case '(': s.Advance(); out.push_back({ETokenType::LParen, "(", loc}); continue;
            case ')': s.Advance(); out.push_back({ETokenType::RParen, ")", loc}); continue;
            case '<': s.Advance(); out.push_back({ETokenType::Lt,     "<", loc}); continue;
            case '>': s.Advance(); out.push_back({ETokenType::Gt,     ">", loc}); continue;
            case '!': s.Advance(); out.push_back({ETokenType::Not,    "!", loc}); continue;
            default:
                s.Throw(std::string("dsl lexer: unexpected character '") + c + "'");
        }
    }
    out.push_back({ETokenType::End, "", s.Loc()});
    return out;
}
