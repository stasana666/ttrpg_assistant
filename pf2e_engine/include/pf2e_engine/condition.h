#pragma once

#include <string>
#include <unordered_map>

enum class ECondition {
    MultipleAttackPenalty,
    Frightened,
    COUNT,
};

std::string ToString(ECondition);
ECondition ConditionFromString(std::string);

using TConditions = std::unordered_map<ECondition, int>;
