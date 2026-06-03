#include <pf2e_engine/dsl/parser.h>

#include <expr/parser.h>

#include <memory>
#include <string>

std::unique_ptr<TDslExpression> ParseDsl(std::string_view src)
{
    return std::make_unique<TDslExpression>(expr::Parse(std::string(src)));
}
