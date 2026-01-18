#pragma once

#include <pf2e_engine/action_blocks/base_function.h>

#include <pf2e_engine/game_object_logic/game_object_id.h>

#include <pf2e_engine/battle_map.h>

class FChooseTarget : public FBaseFunction {
public:
    FChooseTarget(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator ()(std::shared_ptr<TActionContext> ctx) const;

private:
    void EmanationHandle(std::shared_ptr<TActionContext> ctx) const;
    void BurstHandle(std::shared_ptr<TActionContext> ctx) const;
    void ConeHandle(std::shared_ptr<TActionContext> ctx) const;
    void LineHandle(std::shared_ptr<TActionContext> ctx) const;
    void ChooseTarget(std::vector<TPlayer*> players, std::shared_ptr<TActionContext> ctx) const;
};
