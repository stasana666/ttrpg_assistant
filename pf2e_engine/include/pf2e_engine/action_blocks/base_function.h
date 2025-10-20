#pragma once

#include <pf2e_engine/action_blocks/block_input.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>

class FBaseFunction {
public:
    FBaseFunction(TBlockInput&& input, TGameObjectId output);

protected:
    TBlockInput input_;
    TGameObjectId output_;
};
