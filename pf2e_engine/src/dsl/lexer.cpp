#include <pf2e_engine/dsl/lexer.h>

#include <cctype>
#include <stdexcept>
#include <string>

namespace {

bool IsIdentStart(char c) {
    return (std::isalpha(static_cast<unsigned char>(c)) != 0) || c == '_';
}

bool IsIdentCont(char c) {
    return (std::isalnum(static_cast<unsigned char>(c)) != 0) || c == '_';
}

bool IsDigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

bool IsSpace(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

}  // namespace

std::vector<TToken> Tokenize(std::string_view src)
{
    std::vector<TToken> out;
    size_t i = 0;
    auto match2 = [&](char a, char b, ETokenType t) -> bool {
        if (i + 1 < src.size() && src[i] == a && src[i + 1] == b) {
            out.push_back({t, std::string(src.substr(i, 2)), 0});
            i += 2;
            return true;
        }
        return false;
    };

    while (i < src.size()) {
        char c = src[i];
        if (IsSpace(c)) {
            ++i;
            continue;
        }
        if (IsDigit(c)) {
            size_t j = i;
            while (j < src.size() && IsDigit(src[j])) {
                ++j;
            }
            TToken tok{ETokenType::Number, std::string(src.substr(i, j - i)), 0};
            tok.number = std::stoi(tok.text);
            out.push_back(std::move(tok));
            i = j;
            continue;
        }
        if (IsIdentStart(c)) {
            size_t j = i;
            while (j < src.size() && IsIdentCont(src[j])) {
                ++j;
            }
            out.push_back({ETokenType::Identifier, std::string(src.substr(i, j - i)), 0});
            i = j;
            continue;
        }
        // Two-char operators first.
        if (match2('>', '=', ETokenType::Ge)) {
            continue;
        }
        if (match2('<', '=', ETokenType::Le)) {
            continue;
        }
        if (match2('=', '=', ETokenType::Eq)) {
            continue;
        }
        if (match2('!', '=', ETokenType::Ne)) {
            continue;
        }
        if (match2('&', '&', ETokenType::And)) {
            continue;
        }
        if (match2('|', '|', ETokenType::Or)) {
            continue;
        }
        // Single-char tokens.
        switch (c) {
            case '$': out.push_back({ETokenType::Dollar, "$", 0}); ++i; continue;
            case '.': out.push_back({ETokenType::Dot, ".", 0}); ++i; continue;
            case ',': out.push_back({ETokenType::Comma, ",", 0}); ++i; continue;
            case '(': out.push_back({ETokenType::LParen, "(", 0}); ++i; continue;
            case ')': out.push_back({ETokenType::RParen, ")", 0}); ++i; continue;
            case '<': out.push_back({ETokenType::Lt, "<", 0}); ++i; continue;
            case '>': out.push_back({ETokenType::Gt, ">", 0}); ++i; continue;
            case '!': out.push_back({ETokenType::Not, "!", 0}); ++i; continue;
            default:
                throw std::runtime_error("dsl lexer: unexpected character '" + std::string(1, c) + "'");
        }
    }
    out.push_back({ETokenType::End, "", 0});
    return out;
}
