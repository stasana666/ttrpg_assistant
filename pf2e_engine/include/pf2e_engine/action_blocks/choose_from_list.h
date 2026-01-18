#pragma once

#include <pf2e_engine/action_blocks/base_function.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>

class FChooseFromList : public FBaseFunction {
public:
    FChooseFromList(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator()(std::shared_ptr<TActionContext> ctx) const;
};
