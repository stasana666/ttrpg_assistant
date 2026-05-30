#include <pf2e_engine/dsl/ast_nodes.h>

#include <pf2e_engine/dsl/function_registry.h>
#include <pf2e_engine/dsl/property_access.h>
#include <pf2e_engine/common/visit.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/game_object_logic/game_object.h>

#include <stdexcept>

TLiteralExpr::TLiteralExpr(TDslValue value)
    : value_(std::move(value)) {}

TDslValue TLiteralExpr::Evaluate(TEvalContext&) const
{
    return value_;
}

TVariableExpr::TVariableExpr(std::string name)
    : name_(std::move(name)) {}

namespace {

TDslValue FromGameObject(const TGameObjectPtr& obj)
{
    return std::visit(VisitorHelper{
        [](TArmor* p) -> TDslValue { return TDslValue(p); },
        [](TWeapon* p) -> TDslValue { return TDslValue(p); },
        [](TCreature* p) -> TDslValue { return TDslValue(p); },
        [](TPlayer* p) -> TDslValue { return TDslValue(p); },
        [](const TPlayerList& list) -> TDslValue {
            TDslValue::TList items;
            items.reserve(list.size());
            for (TPlayer* p : list) {
                items.emplace_back(p);
            }
            return TDslValue::MakeList(std::move(items));
        },
        [](const TWeaponList& list) -> TDslValue {
            TDslValue::TList items;
            items.reserve(list.size());
            for (TWeapon* w : list) {
                items.emplace_back(w);
            }
            return TDslValue::MakeList(std::move(items));
        },
        [](const TDslValue::TListPtr& list) -> TDslValue {
            return TDslValue(list);
        },
        [](int i) -> TDslValue { return TDslValue(i); },
        [](const std::shared_ptr<TDamage>&) -> TDslValue {
            throw std::runtime_error("dsl: TDamage cannot appear as a DSL value");
        },
        [](const std::string&) -> TDslValue {
            throw std::runtime_error("dsl: string cannot appear as a DSL value");
        },
        [](ESuccessLevel) -> TDslValue {
            throw std::runtime_error("dsl: ESuccessLevel cannot appear as a DSL value");
        }
    }, obj);
}

}  // namespace

TDslValue TVariableExpr::Evaluate(TEvalContext& ctx) const
{
    // Scoped bindings (from filter/map/foldl) shadow the registry.
    auto scoped = ctx.scope.find(name_);
    if (scoped != ctx.scope.end()) {
        return scoped->second;
    }
    TGameObjectId id = TGameObjectIdManager::Instance().Register(name_);
    if (!ctx.registry->Contains(id)) {
        throw std::runtime_error("dsl: variable '$" + name_ + "' not found");
    }
    return FromGameObject(ctx.registry->GetGameObjectPtr(id));
}

TPropertyExpr::TPropertyExpr(std::unique_ptr<IDslExpression> receiver, std::string property)
    : receiver_(std::move(receiver))
    , property_(std::move(property)) {}

TDslValue TPropertyExpr::Evaluate(TEvalContext& ctx) const
{
    TDslValue receiver = receiver_->Evaluate(ctx);
    return GetDslProperty(receiver, property_, ctx);
}

TCallExpr::TCallExpr(std::string name, std::vector<std::unique_ptr<IDslExpression>> args)
    : name_(std::move(name))
    , args_(std::move(args)) {}

TDslValue TCallExpr::Evaluate(TEvalContext& ctx) const
{
    std::vector<TDslValue> evaluated;
    evaluated.reserve(args_.size());
    for (const auto& arg : args_) {
        evaluated.push_back(arg->Evaluate(ctx));
    }
    return TDslFunctionRegistry::Instance().Call(name_, evaluated, ctx);
}

TBinaryExpr::TBinaryExpr(EBinaryOp op, std::unique_ptr<IDslExpression> lhs, std::unique_ptr<IDslExpression> rhs)
    : op_(op)
    , lhs_(std::move(lhs))
    , rhs_(std::move(rhs)) {}

namespace {

bool DslEquals(const TDslValue& a, const TDslValue& b)
{
    if (a.data.index() != b.data.index()) {
        return false;
    }
    return std::visit(VisitorHelper{
        [&](std::monostate) { return true; },
        [&](bool x) { return x == b.AsBool(); },
        [&](int x) { return x == b.AsInt(); },
        [&](TArmor* p) { return p == std::get<TArmor*>(b.data); },
        [&](TWeapon* p) { return p == std::get<TWeapon*>(b.data); },
        [&](TCreature* p) { return p == std::get<TCreature*>(b.data); },
        [&](TPlayer* p) { return p == std::get<TPlayer*>(b.data); },
        [&](const TDslValue::TListPtr&) -> bool {
            throw std::runtime_error("dsl: list equality not supported");
        }
    }, a.data);
}

int RequireInt(const TDslValue& v, const char* op_name)
{
    if (!v.Is<int>()) {
        throw std::runtime_error(std::string("dsl: operator '") + op_name + "' requires int operand");
    }
    return v.AsInt();
}

bool RequireBool(const TDslValue& v, const char* op_name)
{
    if (!v.Is<bool>()) {
        throw std::runtime_error(std::string("dsl: operator '") + op_name + "' requires bool operand");
    }
    return v.AsBool();
}

}  // namespace

TDslValue TBinaryExpr::Evaluate(TEvalContext& ctx) const
{
    // Short-circuit && and ||
    if (op_ == EBinaryOp::And) {
        TDslValue lhs = lhs_->Evaluate(ctx);
        if (!RequireBool(lhs, "&&")) {
            return TDslValue(false);
        }
        return TDslValue(RequireBool(rhs_->Evaluate(ctx), "&&"));
    }
    if (op_ == EBinaryOp::Or) {
        TDslValue lhs = lhs_->Evaluate(ctx);
        if (RequireBool(lhs, "||")) {
            return TDslValue(true);
        }
        return TDslValue(RequireBool(rhs_->Evaluate(ctx), "||"));
    }

    TDslValue lhs = lhs_->Evaluate(ctx);
    TDslValue rhs = rhs_->Evaluate(ctx);

    switch (op_) {
        case EBinaryOp::Eq: return TDslValue(DslEquals(lhs, rhs));
        case EBinaryOp::Ne: return TDslValue(!DslEquals(lhs, rhs));
        case EBinaryOp::Lt: return TDslValue(RequireInt(lhs, "<") < RequireInt(rhs, "<"));
        case EBinaryOp::Le: return TDslValue(RequireInt(lhs, "<=") <= RequireInt(rhs, "<="));
        case EBinaryOp::Gt: return TDslValue(RequireInt(lhs, ">") > RequireInt(rhs, ">"));
        case EBinaryOp::Ge: return TDslValue(RequireInt(lhs, ">=") >= RequireInt(rhs, ">="));
        case EBinaryOp::And:
        case EBinaryOp::Or:
            break;  // handled above
    }
    throw std::logic_error("dsl: unreachable binary op");
}

TUnaryNotExpr::TUnaryNotExpr(std::unique_ptr<IDslExpression> inner)
    : inner_(std::move(inner)) {}

TDslValue TUnaryNotExpr::Evaluate(TEvalContext& ctx) const
{
    TDslValue v = inner_->Evaluate(ctx);
    return TDslValue(!RequireBool(v, "!"));
}
