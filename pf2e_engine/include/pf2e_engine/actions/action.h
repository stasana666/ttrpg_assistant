#pragma once

#include "action_block.h"
#include "action_context.h"

class TAction {
public:
    using TPipeline = std::vector<std::unique_ptr<IActionBlock>>;

    explicit TAction(TPipeline&& pipeline, std::string&& name);

    void Apply(TActionContext& ctx);
    bool Check(const TPlayer& self);
    std::string_view Name() const;

private:
    TPipeline pipeline_;
    std::string name_;
};
