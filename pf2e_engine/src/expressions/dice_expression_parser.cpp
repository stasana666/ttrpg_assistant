#include "dice_expression_parser.h"
#include "multi_dice_expression.h"

#include <stdexcept>
#include <cctype>

std::unique_ptr<IExpression> ParseDiceExpression(const std::string& expr)
{
    // Parse "NdM" format where N is the count and M is the die size
    // Examples: "6d6", "2d8", "1d12"

    size_t d_pos = expr.find('d');
    if (d_pos == std::string::npos) {
        throw std::invalid_argument("Invalid dice expression: missing 'd' in '" + expr + "'");
    }

    std::string count_str = expr.substr(0, d_pos);
    std::string size_str = expr.substr(d_pos + 1);

    if (count_str.empty() || size_str.empty()) {
        throw std::invalid_argument("Invalid dice expression: '" + expr + "'");
    }

    int count = std::stoi(count_str);
    int size = std::stoi(size_str);

    if (count <= 0 || size <= 0) {
        throw std::invalid_argument("Invalid dice expression: count and size must be positive in '" + expr + "'");
    }

    return std::make_unique<TMultiDiceExpression>(count, size);
}
