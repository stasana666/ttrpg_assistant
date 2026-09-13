#pragma once


#include <parse/token_stream.h>

#include <string>
#include <vector>

namespace expr {

enum class ETok {
    IntLiteral,
    Ident,
    Dollar,
    Dot,
    Comma,
    LParen, RParen,
    Plus, Minus, Star, Slash,
    Lt, Le, Gt, Ge, EqEq, BangEq, AmpAmp, PipePipe, Bang,
    End,
    At,
    LBrace, RBrace,
    LBracket, RBracket,
    Assign,
    Semicolon,
    StringLiteral,
};

using TToken = parse::TToken<ETok>;

std::vector<TToken> Tokenize(const std::string& src);

}
