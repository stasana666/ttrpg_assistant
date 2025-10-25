#pragma once

#include <string>

enum class ESuccessLevel {
    CriticalFailure,
    Failure,
    Success,
    CriticalSuccess,
};

std::string ToString(ESuccessLevel success_level);

ESuccessLevel IncreaseSuccessLevel(ESuccessLevel lvl);

ESuccessLevel DecreaseSuccessLevel(ESuccessLevel lvl);
