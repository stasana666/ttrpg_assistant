#pragma once

#include <pf2e_engine/action_blocks/base_function.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>

#include <string>

class FContributeDamageBonus : public FBaseFunction {
public:
    FContributeDamageBonus(TBlockInput&& input, TGameObjectId output);

    void operator()(std::shared_ptr<TActionContext> ctx) const;

private:
    std::string dice_;
    std::string damage_type_;
};
