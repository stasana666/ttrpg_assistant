#pragma once

#include <pf2e_engine/action_blocks/base_function.h>

#include <pf2e_engine/game_object_logic/game_object_id.h>

#include <pf2e_engine/battle_map.h>

class FChooseTarget : public FBaseFunction {
public:
    FChooseTarget(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator ()(TActionContext& ctx) const;

private:
    void EmanationHandle(TActionContext& ctx) const;
    void ChooseTarget(std::vector<TPlayer*> players, TActionContext& ctx) const;
};
