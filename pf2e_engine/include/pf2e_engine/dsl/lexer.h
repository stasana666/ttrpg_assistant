#pragma once

#include <string>
#include <string_view>
#include <vector>

enum class ETokenType {
    Number,
    Identifier,
    Dollar,
    Dot,
    Comma,
    LParen,
    RParen,
    Lt, Le, Gt, Ge, Eq, Ne,
    And, Or, Not,
    End,
};

struct TToken {
    ETokenType type;
    std::string text;   // identifier name; numeric literal text
    int number = 0;     // parsed value when type == Number
};

std::vector<TToken> Tokenize(std::string_view src);
