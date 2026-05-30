#include <foldl.h>

#include <pf2e_engine/dsl/builtins.h>
#include <pf2e_engine/dsl/parser.h>
#include <pf2e_engine/dsl/value_convert.h>

#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>

#include <stdexcept>

static const TGameObjectId kListId = TGameObjectIdManager::Instance().Register("list");
static const TGameObjectId kExpressionId = TGameObjectIdManager::Instance().Register("expression");
static const TGameObjectId kInitId = TGameObjectIdManager::Instance().Register("init");

FFoldL::FFoldL(TBlockInput&& input, TGameObjectId output)
    : FBaseFunction(std::move(input), output)
{
    EnsureDslBuiltinsRegistered();
    expr_ = std::shared_ptr<IDslExpression>(ParseDsl(input_.GetString(kExpressionId)));
    if (input_.Has(kInitId)) {
        init_ = std::shared_ptr<IDslExpression>(ParseDsl(input_.GetString(kInitId)));
    }
}

void FFoldL::operator()(std::shared_ptr<TActionContext> ctx) const
{
    TDslValue::TList items = AsDslList(input_.Get(kListId, ctx));

    TEvalContext ectx;
    ectx.registry = ctx->game_object_registry;
    ectx.battle = ctx->battle;

    size_t start = 0;
    TDslValue acc;
    if (init_) {
        acc = init_->Evaluate(ectx);
    } else {
        if (items.empty()) {
            throw std::runtime_error("dsl foldl: empty list with no init");
        }
        acc = items[0];
        start = 1;
    }

    TScopeGuard acc_guard(ectx, "acc", acc);
    for (size_t i = start; i < items.size(); ++i) {
        TScopeGuard item_guard(ectx, "item", items[i]);
        acc = expr_->Evaluate(ectx);
        acc_guard.Rebind(acc);
    }

    ctx->game_object_registry->Add(output_, ToGameObjectPtr(acc));
}
