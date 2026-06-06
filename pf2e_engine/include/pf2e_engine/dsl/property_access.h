#pragma once

#include <pf2e_engine/dsl/value.h>
#include <pf2e_engine/dsl/expression.h>

#include <string>

TDslValue GetDslProperty(const TDslValue& receiver, const std::string& name, TEvalContext& ctx);
