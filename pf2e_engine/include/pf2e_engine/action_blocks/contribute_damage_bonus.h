#pragma once

#include <pf2e_engine/action_blocks/base_function.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>

#include <string>

// Adds a fixed dice expression (literal "NdM") of a given damage type into the
// $damage_bonus accumulator that the damage-roll block exposes. Used by feat
// pipelines to contribute conditional bonus damage.
class FContributeDamageBonus : public FBaseFunction {
public:
    FContributeDamageBonus(TBlockInput&& input, TGameObjectId output);

    void operator()(std::shared_ptr<TActionContext> ctx) const;

private:
    std::string dice_;
    std::string damage_type_;
};
