#include <action.h>

#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/generated_history.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/i_interaction_system.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/transformation/transformator.h>

#include <stdexcept>

namespace {

void RunValidatedAbility(const std::shared_ptr<TActionContext>& ctx,
                         const TSavepointCallback& callback)
{
    ValidateAbilitySource(ctx);
    ValidateActionActor(ctx);
    try {
        callback();
    } catch (TSavepointStackUnwind& save_point) {
        save_point.AddCallFunctionLevel([ctx](TSavepointCallback resume) {
            RunValidatedAbility(ctx, resume);
        });
        throw;
    }
}

}

TAction::TAction(FBody body, TBoundResources costs, std::string name)
    : body_(std::move(body))
    , payment_(std::move(costs))
    , name_(std::move(name))
{
    if (!body_) {
        throw std::invalid_argument("generated action body must not be empty");
    }
}

TAction::TAction(TCompiledBody body, TBoundResources costs, std::string name)
    : TAction(std::move(body.Run), std::move(costs), std::move(name))
{
    compiled_body_ = true;
}

void TAction::Apply(std::shared_ptr<TActionContext> ctx, TPlayer& player) const
{
    if (!ctx || ctx->transformator == nullptr) {
        throw std::invalid_argument("action requires an execution context and journal");
    }
    ctx->execution_mode = EActionExecutionMode::Ordinary;
    ctx->owned_lifecycle.reset();
    if (player.GetCreature() == nullptr || !player.GetCreature()->CanAct() || !Check(player)) {
        throw TAbilityUnavailable();
    }
    ValidateAbilitySource(ctx);
    if (ctx->previous_record && (!*ctx->previous_record
        || !MatchActionRecordStamp(TGeneratedWorldView(ctx->battle), player, *ctx->previous_record,
                                    ctx->previous_record->Record()->Id()))) {
        throw TAbilityUnavailable();
    }
    ctx->actor = &player;

    {
        auto execution = ctx->transformator->ExecutionContext();
        auto transaction = execution.Atomic();
        Consume(ctx, player);
        AcceptActionExecution(ctx, player, ctx->history_kind);
        transaction.Commit();
    }

    ctx->io_system->GameLog() << player.GetName() << ": " << name_ << std::endl;

    if (compiled_body_) {
        body_(ctx);
    } else {
        RunValidatedAbility(ctx, [this, ctx]() { body_(ctx); });
    }
}

void TAction::Consume(std::shared_ptr<TActionContext> ctx, TPlayer& player) const
{
    if (!ctx || ctx->transformator == nullptr) {
        throw std::invalid_argument("resource payment requires a journal");
    }
    payment_.Consume(*ctx->transformator, player);
}

bool TAction::Check(const TPlayer& player) const
{
    return payment_.Check(player);
}

std::string_view TAction::Name() const
{
    return name_;
}
