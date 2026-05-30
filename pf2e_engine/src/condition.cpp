#include <pf2e_engine/condition.h>

#include <pf2e_engine/common/ast/ast_helpers.h>

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

std::string ToString(ECondition condition)
{
    switch (condition) {
        case ECondition::MultipleAttackPenalty:
            return "MultipleAttackPenalty";
        case ECondition::Frightened:
            return "Frightened";
        case ECondition::Prone:
            return "Prone";
        case ECondition::COUNT:
            throw std::invalid_argument("COUNT is not valid ECondition");
    }
    throw std::invalid_argument("incorrect value of ECondition");
}

ECondition ConditionFromString(std::string condition)
{
    for (size_t i = 0; i < static_cast<size_t>(ECondition::COUNT); ++i) {
        if (ToString(static_cast<ECondition>(i)) == condition) {
            return static_cast<ECondition>(i);
        }
    }
    throw std::invalid_argument("unknown ECondition: \"" + condition + "\"");
}

TAstNode GetConditionsAst(const TConditions& conditions)
{
    std::vector<std::pair<ECondition, int>> sorted;
    sorted.reserve(conditions.size());
    for (const auto& [cond, value] : conditions) {
        if (value != 0) {
            sorted.emplace_back(cond, value);
        }
    }
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    TAstNode node = TAstNode::MakeObject("TConditions");
    for (const auto& [cond, value] : sorted) {
        AddValueField(node, ToString(cond), value);
    }
    return node;
}
