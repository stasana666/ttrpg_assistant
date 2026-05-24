#pragma once

#include <pf2e_engine/common/ast/ast_node.h>

#include <string>
#include <unordered_map>

enum class ECondition {
    MultipleAttackPenalty,
    Frightened,
    Prone,
    COUNT,
};

std::string ToString(ECondition);
ECondition ConditionFromString(std::string);

using TConditions = std::unordered_map<ECondition, int>;

// TConditions is a typedef, so it cannot carry a member GetAst. This free
// function emits the conditions as an AST object node, sorted by the
// underlying ECondition enum value for determinism.
TAstNode GetConditionsAst(const TConditions& conditions);
