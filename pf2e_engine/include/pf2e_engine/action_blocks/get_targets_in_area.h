#pragma once

#include <pf2e_engine/action_blocks/base_function.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object.h>

class FGetTargetsInArea : public FBaseFunction {
public:
    FGetTargetsInArea(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator()(std::shared_ptr<TActionContext> ctx) const;

private:
    TPlayerList GetEmanationTargets(std::shared_ptr<TActionContext> ctx) const;
    TPlayerList GetBurstTargets(std::shared_ptr<TActionContext> ctx) const;
    TPlayerList GetConeTargets(std::shared_ptr<TActionContext> ctx) const;
    TPlayerList GetLineTargets(std::shared_ptr<TActionContext> ctx) const;
};
