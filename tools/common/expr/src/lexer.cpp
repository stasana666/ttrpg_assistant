#include <expr/lexer.h>

#include <parse/scanner.h>

#include <string>

namespace expr {
namespace {

bool CanEndOperand(ETok kind) {
    switch (kind) {
        case ETok::IntLiteral:
        case ETok::StringLiteral:
        case ETok::Ident:
        case ETok::RParen:
        case ETok::RBrace:
        case ETok::RBracket:
            return true;
        default:
            return false;
    }
}

std::string ScanString(parse::TScanner& scanner) {
    const auto start = scanner.Loc();
    scanner.Advance();
    std::string value;
    while (!scanner.Eof()) {
        const char ch = scanner.Peek();
        scanner.Advance();
        if (ch == '"') {
            return value;
        }
        if (ch == '\n' || ch == '\r') {
            scanner.ThrowAt(start, "newline inside string literal");
        }
        if (ch != '\\') {
            value += ch;
            continue;
        }
        const auto escape_location = scanner.Loc();
        const char escaped = scanner.Peek();
        if (scanner.Eof()) {
            break;
        }
        scanner.Advance();
        switch (escaped) {
            case '"': value += '"'; break;
            case '\\': value += '\\'; break;
            case 'n': value += '\n'; break;
            case 'r': value += '\r'; break;
            case 't': value += '\t'; break;
            default: scanner.ThrowAt(escape_location, "unsupported string escape");
        }
    }
    scanner.ThrowAt(start, "unterminated string literal");
}

}

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
        const bool signed_literal = c == '-' && parse::IsDigit(s.PeekAt(1)) &&
            (out.empty() || !CanEndOperand(out.back().kind));
        if (parse::IsDigit(c) || signed_literal) {
            out.push_back({ETok::IntLiteral, s.ScanInteger(), loc});
            continue;
        }
        if (c == '"') {
            out.push_back({ETok::StringLiteral, ScanString(s), loc});
            continue;
        }

        if (s.Try(">=")) { out.push_back({ETok::Ge,       ">=", loc}); continue; }
        if (s.Try("<=")) { out.push_back({ETok::Le,       "<=", loc}); continue; }
        if (s.Try("==")) { out.push_back({ETok::EqEq,     "==", loc}); continue; }
        if (s.Try("!=")) { out.push_back({ETok::BangEq,   "!=", loc}); continue; }
        if (s.Try("&&")) { out.push_back({ETok::AmpAmp,   "&&", loc}); continue; }
        if (s.Try("||")) { out.push_back({ETok::PipePipe, "||", loc}); continue; }

        switch (c) {
            case '@': s.Advance(); out.push_back({ETok::At,     "@", loc}); continue;
            case '.': s.Advance(); out.push_back({ETok::Dot,    ".", loc}); continue;
            case ',': s.Advance(); out.push_back({ETok::Comma,  ",", loc}); continue;
            case '(': s.Advance(); out.push_back({ETok::LParen, "(", loc}); continue;
            case ')': s.Advance(); out.push_back({ETok::RParen, ")", loc}); continue;
            case '{': s.Advance(); out.push_back({ETok::LBrace, "{", loc}); continue;
            case '}': s.Advance(); out.push_back({ETok::RBrace, "}", loc}); continue;
            case '[': s.Advance(); out.push_back({ETok::LBracket, "[", loc}); continue;
            case ']': s.Advance(); out.push_back({ETok::RBracket, "]", loc}); continue;
            case '=': s.Advance(); out.push_back({ETok::Assign, "=", loc}); continue;
            case ';': s.Advance(); out.push_back({ETok::Semicolon, ";", loc}); continue;
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

}
