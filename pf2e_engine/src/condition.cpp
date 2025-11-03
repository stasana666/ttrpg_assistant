#include <pf2e_engine/condition.h>

#include <stdexcept>

std::string ToString(ECondition condition)
{
    switch (condition) {
        case ECondition::MultipleAttackPenalty:
            return "MultipleAttackPenalty";
        case ECondition::Frightened:
            return "Frightened";
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
