#include <pf2e_engine/condition.h>

#include <stdexcept>

std::string ToString(ECondition condition)
{
    switch (condition) {
        case ECondition::MultipleAttackPenalty:
            return "MultipleAttackPenalty";
        case ECondition::COUNT:
            throw std::runtime_error("COUNT is not valid ECondition");
    }
    throw std::runtime_error("incorrect value of ECondition");
}

ECondition ConditionFromString(std::string condition)
{
    for (size_t i = 0; i < static_cast<size_t>(ECondition::COUNT); ++i) {
        if (ToString(static_cast<ECondition>(i)) == condition) {
            return static_cast<ECondition>(i);
        }
    }
    throw std::runtime_error("unknown ECondition: \"" + condition + "\"");
}
