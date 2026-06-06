#pragma once

// Tokenizer for the shared expression grammar. Built on the grammar-agnostic
// parse::TScanner. The token set is the union of what the DSL and the codegen
// need: arithmetic, comparison, logical, member access (`.`), calls, `$`-vars.

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
};

using TToken = parse::TToken<ETok>;

std::vector<TToken> Tokenize(const std::string& src);

}  // namespace expr
