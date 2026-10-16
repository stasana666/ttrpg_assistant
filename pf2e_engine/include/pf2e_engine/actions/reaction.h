#pragma once

#include <pf2e_engine/actions/action.h>

#include <string>

enum class ETrigger {
    OnMove,
    COUNT,
};

struct TTriggerContext {
    ETrigger type;
    const TPlayer* triggered_player;
};

std::string ToString(ETrigger trigger);
ETrigger TriggerFromString(const std::string& trigger_str);
