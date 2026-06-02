#include <ttrpg/parser.h>

std::vector<TToken> Tokenize(std::string src) {
    parse::TScanner s(std::move(src));
    std::vector<TToken> out;
    while (true) {
        // Skip whitespace + `//` line comments until a real token or EOF.
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
        if (parse::IsDigit(c) || (c == '-' && parse::IsDigit(s.PeekAt(1)))) {
            out.push_back({ETok::IntLiteral, s.ScanInteger(), loc});
            continue;
        }
        if (c == '"') {
            out.push_back({ETok::StringLiteral, s.ScanQuotedString(), loc});
            continue;
        }
        switch (c) {
            case '{': s.Advance(); out.push_back({ETok::LBrace, "{", loc}); continue;
            case '}': s.Advance(); out.push_back({ETok::RBrace, "}", loc}); continue;
            case '<': s.Advance(); out.push_back({ETok::LAngle, "<", loc}); continue;
            case '>': s.Advance(); out.push_back({ETok::RAngle, ">", loc}); continue;
            case ';': s.Advance(); out.push_back({ETok::Semi,   ";", loc}); continue;
            case ',': s.Advance(); out.push_back({ETok::Comma,  ",", loc}); continue;
            case '=': s.Advance(); out.push_back({ETok::Equals, "=", loc}); continue;
            default:
                s.Throw(std::string("unexpected character '") + c + "'");
        }
    }
    out.push_back({ETok::End, "", s.Loc()});
    return out;
}
