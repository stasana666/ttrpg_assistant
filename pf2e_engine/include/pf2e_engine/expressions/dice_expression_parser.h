#pragma once

#include <pf2e_engine/expressions/base_expression.h>

#include <memory>
#include <string>

std::unique_ptr<IExpression> ParseDiceExpression(const std::string& expr);