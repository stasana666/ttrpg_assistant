#pragma once

#include <pf2e_engine/actions/action.h>

#include <string>

enum class ETrigger {
    OnMove,
    COUNT,
};

struct TTriggerContext {
    ETrigger type;
    TPlayer* triggered_player;
};

std::string ToString(ETrigger trigger);
ETrigger TriggerFromString(const std::string& trigger_str);

class TReaction : public TAction {
public:
    ETrigger TriggerType() const;
    bool Check(std::shared_ptr<TActionContext> ctx) const;

private:
    ETrigger trigger_;
    TPipeline trigger_pipeline_;
};
