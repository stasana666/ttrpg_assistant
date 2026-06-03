#include <expr/lexer.h>

#include <parse/scanner.h>

#include <string>

namespace expr {

std::vector<TToken> Tokenize(const std::string& src) {
    parse::TScanner s{src};
    std::vector<TToken> out;

    while (true) {
        s.SkipWhitespace();
        if (s.Eof()) {
            break;
        }
        parse::TSourceLocation loc = s.Loc();
        char c = s.Peek();

        if (parse::IsIdentStart(c)) {
            out.push_back({ETok::Ident, s.ScanIdent(), loc});
            continue;
        }
        // A '-' directly in front of a digit is part of a negative literal;
        // otherwise it is binary subtraction (handled below).
        if (parse::IsDigit(c) || (c == '-' && parse::IsDigit(s.PeekAt(1)))) {
            out.push_back({ETok::IntLiteral, s.ScanInteger(), loc});
            continue;
        }

        // Two-char operators before their one-char prefixes.
        if (s.Try(">=")) { out.push_back({ETok::Ge,       ">=", loc}); continue; }
        if (s.Try("<=")) { out.push_back({ETok::Le,       "<=", loc}); continue; }
        if (s.Try("==")) { out.push_back({ETok::EqEq,     "==", loc}); continue; }
        if (s.Try("!=")) { out.push_back({ETok::BangEq,   "!=", loc}); continue; }
        if (s.Try("&&")) { out.push_back({ETok::AmpAmp,   "&&", loc}); continue; }
        if (s.Try("||")) { out.push_back({ETok::PipePipe, "||", loc}); continue; }

        switch (c) {
            case '$': s.Advance(); out.push_back({ETok::Dollar, "$", loc}); continue;
            case '.': s.Advance(); out.push_back({ETok::Dot,    ".", loc}); continue;
            case ',': s.Advance(); out.push_back({ETok::Comma,  ",", loc}); continue;
            case '(': s.Advance(); out.push_back({ETok::LParen, "(", loc}); continue;
            case ')': s.Advance(); out.push_back({ETok::RParen, ")", loc}); continue;
            case '+': s.Advance(); out.push_back({ETok::Plus,   "+", loc}); continue;
            case '-': s.Advance(); out.push_back({ETok::Minus,  "-", loc}); continue;
            case '*': s.Advance(); out.push_back({ETok::Star,   "*", loc}); continue;
            case '/': s.Advance(); out.push_back({ETok::Slash,  "/", loc}); continue;
            case '<': s.Advance(); out.push_back({ETok::Lt,     "<", loc}); continue;
            case '>': s.Advance(); out.push_back({ETok::Gt,     ">", loc}); continue;
            case '!': s.Advance(); out.push_back({ETok::Bang,   "!", loc}); continue;
            default:
                s.Throw(std::string("expr lexer: unexpected character '") + c + "'");
        }
    }
    out.push_back({ETok::End, "", s.Loc()});
    return out;
}

}  // namespace expr
