#pragma once

#include "action_block.h"
#include "action_context.h"

class TAction {
public:
    void Apply(TActionContext& ctx);
    bool Check(const TActionContext& ctx);

private:
    IActionBlock* begin_;
};
