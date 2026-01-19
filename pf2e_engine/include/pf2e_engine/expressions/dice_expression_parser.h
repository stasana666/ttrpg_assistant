#pragma once

#include <pf2e_engine/expressions/base_expression.h>

#include <memory>
#include <string>

// Parses dice expressions in "NdM" format (e.g., "6d6", "2d8")
// Returns a TMultiDiceExpression
std::unique_ptr<IExpression> ParseDiceExpression(const std::string& expr);