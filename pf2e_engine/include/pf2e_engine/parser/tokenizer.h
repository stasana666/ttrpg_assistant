#pragma once

#include <variant>

namespace token {

struct TNumber {
    int value;
};

enum class EOperation {
    Add,
    Mul,
};

enum class EBracket {
    Open,
    Close,
};


using TToken = std::variant<TNumber, EOperation, EBracket>;

};

class TTokenizer {

};
