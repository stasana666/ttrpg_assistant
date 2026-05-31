#pragma once

#include <parse/token_stream.h>

#include <string>
#include <string_view>
#include <vector>

enum class ETokenType {
    Number,
    Identifier,
    Dollar,
    Dot,
    Comma,
    LParen, RParen,
    Lt, Le, Gt, Ge, Eq, Ne,
    And, Or, Not,
    End,
};

using TToken = parse::TToken<ETokenType>;

std::vector<TToken> Tokenize(std::string_view src);
