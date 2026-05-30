#include <filter.h>

#include <pf2e_engine/dsl/builtins.h>
#include <pf2e_engine/dsl/parser.h>
#include <pf2e_engine/dsl/value_convert.h>

#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>

#include <stdexcept>

static const TGameObjectId kListId = TGameObjectIdManager::Instance().Register("list");
static const TGameObjectId kPredicateId = TGameObjectIdManager::Instance().Register("predicate");

FFilter::FFilter(TBlockInput&& input, TGameObjectId output)
    : FBaseFunction(std::move(input), output)
{
    EnsureDslBuiltinsRegistered();
    predicate_ = std::shared_ptr<IDslExpression>(ParseDsl(input_.GetString(kPredicateId)));
}

void FFilter::operator()(std::shared_ptr<TActionContext> ctx) const
{
    TDslValue::TList items = AsDslList(input_.Get(kListId, ctx));

    TEvalContext ectx;
    ectx.registry = ctx->game_object_registry;
    ectx.battle = ctx->battle;

    TDslValue::TList kept;
    for (const TDslValue& item : items) {
        TScopeGuard guard(ectx, "item", item);
        TDslValue v = predicate_->Evaluate(ectx);
        if (!v.Is<bool>()) {
            throw std::runtime_error("dsl filter: predicate must return bool");
        }
        if (v.AsBool()) {
            kept.push_back(item);
        }
    }
    ctx->game_object_registry->Add(output_, ToGameObjectPtr(TDslValue::MakeList(std::move(kept))));
}
