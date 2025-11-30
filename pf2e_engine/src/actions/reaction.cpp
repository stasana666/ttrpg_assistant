#include "reaction.h"

#include <stdexcept>

std::string ToString(ETrigger trigger)
{
    switch (trigger) {
        case ETrigger::OnMove:
            return "on_move";
        case ETrigger::COUNT:
            throw std::invalid_argument("COUNT is not valid ETrigger");
    }
    throw std::invalid_argument("incorrect value of ETrigger");
}

ETrigger TriggerFromString(const std::string& trigger_str)
{
    for (size_t i = 0; i < static_cast<size_t>(ETrigger::COUNT); ++i) {
        if (ToString(static_cast<ETrigger>(i)) == trigger_str) {
            return static_cast<ETrigger>(i);
        }
    }
    throw std::invalid_argument("unknown ETrigger: \"" + trigger_str + "\"");
}

ETrigger TReaction::TriggerType() const
{
    return trigger_;
}

bool TReaction::Check(std::shared_ptr<TActionContext>) const
{
    return true;
}
