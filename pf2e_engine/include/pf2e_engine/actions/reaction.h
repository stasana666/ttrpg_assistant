#pragma once

#include <pf2e_engine/actions/action.h>

enum class ETrigger {
    OnMove,
};

struct TTriggerContext {
    ETrigger type;
    const TPlayer* triggered_player;
};
