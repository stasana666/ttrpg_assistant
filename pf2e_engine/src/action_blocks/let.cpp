#include <let.h>

#include <pf2e_engine/dsl/builtins.h>
#include <pf2e_engine/dsl/parser.h>
#include <pf2e_engine/dsl/value_convert.h>

#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>

static const TGameObjectId kExpressionId = TGameObjectIdManager::Instance().Register("expression");

FLet::FLet(TBlockInput&& input, TGameObjectId output)
    : FBaseFunction(std::move(input), output)
{
    EnsureDslBuiltinsRegistered();
    expr_ = std::shared_ptr<IDslExpression>(ParseDsl(input_.GetString(kExpressionId)));
}

void FLet::operator()(std::shared_ptr<TActionContext> ctx) const
{
    TEvalContext ectx;
    ectx.registry = ctx->game_object_registry;
    ectx.battle = ctx->battle;
    TDslValue result = expr_->Evaluate(ectx);
    ctx->game_object_registry->Add(output_, ToGameObjectPtr(result));
}
