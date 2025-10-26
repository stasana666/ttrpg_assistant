#include <action.h>

#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/player.h>

TAction::TAction(TPipeline&& pipeline, std::string&& name)
    : pipeline_(std::move(pipeline))
    , name_(std::move(name))
{
}

void TAction::Apply(TActionContext& ctx)
{
    ctx.next_block = pipeline_.begin()->get();
    while (ctx.next_block != nullptr) {
        ctx.next_block->Apply(ctx);
    }
}

bool TAction::Check(const TPlayer&)
{
    return true;
}

std::string_view TAction::Name() const
{
    return name_;
}
