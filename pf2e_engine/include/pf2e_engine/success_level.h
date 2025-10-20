#pragma once

#include <algorithm>
#include <stdexcept>
#include <string>

enum class ESuccessLevel {
    CriticalFailure,
    Failure,
    Success,
    CriticalSuccess,
};

std::string ToString(ESuccessLevel success_level)
{
    switch (success_level)
    {
        case ESuccessLevel::CriticalFailure:
            return "critical_failure";
        case ESuccessLevel::Failure:
            return "failure";
        case ESuccessLevel::Success:
            return "success";
        case ESuccessLevel::CriticalSuccess:
            return "critical_success";
    }
    throw std::logic_error("unknown enum ESuccessLevel value");
}

ESuccessLevel IncreaseSuccessLevel(ESuccessLevel lvl)
{
    return static_cast<ESuccessLevel>(std::min(3, static_cast<int>(lvl) + 1));
}

ESuccessLevel DecreaseSuccessLevel(ESuccessLevel lvl)
{
    return static_cast<ESuccessLevel>(std::max(0, static_cast<int>(lvl) - 1));
}
