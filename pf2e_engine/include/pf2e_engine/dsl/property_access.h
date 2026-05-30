#pragma once

#include <pf2e_engine/dsl/value.h>
#include <pf2e_engine/dsl/expression.h>

#include <string>

// Visits the receiver's variant and dispatches to the matching
// TPropertyRegistry<T>::Get. Throws on receivers that don't support property
// access (monostate, bool, int, list).
TDslValue GetDslProperty(const TDslValue& receiver, const std::string& name, TEvalContext& ctx);
