#pragma once

#include <pf2e_engine/action_blocks/base_function.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>

// Outputs ESuccessLevel::Success if some ally (a player on the same team as
// $self, other than $self) is alive and adjacent to $target; otherwise
// ESuccessLevel::Failure. The existing Switch block branches on the output.
class FCheckAllyAdjacent : public FBaseFunction {
public:
    FCheckAllyAdjacent(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator()(std::shared_ptr<TActionContext> ctx) const;
};
