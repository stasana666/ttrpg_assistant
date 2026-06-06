#pragma once

#include <pf2e_engine/dsl/expression.h>

#include <memory>
#include <string_view>

std::unique_ptr<TDslExpression> ParseDsl(std::string_view src);
