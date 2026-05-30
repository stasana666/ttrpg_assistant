#include <map.h>

#include <pf2e_engine/dsl/builtins.h>
#include <pf2e_engine/dsl/parser.h>
#include <pf2e_engine/dsl/value_convert.h>

#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>

static const TGameObjectId kListId = TGameObjectIdManager::Instance().Register("list");
static const TGameObjectId kExpressionId = TGameObjectIdManager::Instance().Register("expression");

FMap::FMap(TBlockInput&& input, TGameObjectId output)
    : FBaseFunction(std::move(input), output)
{
    EnsureDslBuiltinsRegistered();
    expr_ = std::shared_ptr<IDslExpression>(ParseDsl(input_.GetString(kExpressionId)));
}

void FMap::operator()(std::shared_ptr<TActionContext> ctx) const
{
    TDslValue::TList items = AsDslList(input_.Get(kListId, ctx));

    TEvalContext ectx;
    ectx.registry = ctx->game_object_registry;
    ectx.battle = ctx->battle;

    TDslValue::TList mapped;
    mapped.reserve(items.size());
    for (const TDslValue& item : items) {
        TScopeGuard guard(ectx, "item", item);
        mapped.push_back(expr_->Evaluate(ectx));
    }
    ctx->game_object_registry->Add(output_, ToGameObjectPtr(TDslValue::MakeList(std::move(mapped))));
}
