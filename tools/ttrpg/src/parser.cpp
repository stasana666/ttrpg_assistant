#include <ttrpg/parser.h>

std::vector<TToken> Tokenize(std::string src) {
    parse::TScanner s(std::move(src));
    std::vector<TToken> out;
    while (true) {
        for (;;) {
            s.SkipWhitespace();
            if (!s.SkipLineComment("//")) {
                break;
            }
        }
        if (s.Eof()) {
            break;
        }
        parse::TSourceLocation loc = s.Loc();
        char c = s.Peek();
        if (parse::IsIdentStart(c)) {
            out.push_back({ETok::Ident, s.ScanIdent(), loc});
            continue;
        }
        if (parse::IsDigit(c)) {
            out.push_back({ETok::IntLiteral, s.ScanInteger(), loc});
            continue;
        }
        if (c == '"') {
            s.Advance();
            std::string value;
            while (!s.Eof() && s.Peek() != '"') {
                if (s.Peek() == '\n') s.Throw("newline inside string literal");
                if (s.Peek() == '\\') {
                    value += s.Peek();
                    s.Advance();
                    if (s.Eof()) s.Throw("unterminated string literal");
                }
                value += s.Peek();
                s.Advance();
            }
            if (!s.Try('"')) s.Throw("unterminated string literal");
            out.push_back({ETok::StringLiteral, std::move(value), loc});
            continue;
        }
        const std::string pair{c, s.PeekAt(1)};
        if (pair == "+=" || pair == "-=" || pair == "==" || pair == "!=" ||
            pair == "<=" || pair == ">=" || pair == "&&" || pair == "||") {
            s.Advance();
            s.Advance();
            out.push_back({ETok::Operator, pair, loc});
            continue;
        }
        ETok kind;
        switch (c) {
            case '{': kind = ETok::LBrace; break;
            case '}': kind = ETok::RBrace; break;
            case '<': kind = ETok::LAngle; break;
            case '>': kind = ETok::RAngle; break;
            case ';': kind = ETok::Semi; break;
            case ',': kind = ETok::Comma; break;
            case '=': kind = ETok::Equals; break;
            case '(': kind = ETok::LParen; break;
            case ')': kind = ETok::RParen; break;
            case '[': kind = ETok::LBracket; break;
            case ']': kind = ETok::RBracket; break;
            case '@': case '$': case '.': case '+': case '-': case '*': case '/': case '!':
                kind = ETok::Operator;
                break;
            default: s.Throw(std::string("unexpected character '") + c + "'");
        }
        s.Advance();
        out.push_back({kind, std::string(1, c), loc});
    }
    out.push_back({ETok::End, "", s.Loc()});
    return out;
}
