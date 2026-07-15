#include <pf2e_engine/dsl/expression.h>

#include <pf2e_engine/dsl/function_registry.h>
#include <pf2e_engine/dsl/property_access.h>
#include <pf2e_engine/dsl/value.h>
#include <pf2e_engine/common/visit.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/game_object_logic/game_object.h>

#include <expr/ast.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace {

TDslValue FromGameObject(const TGameObjectPtr& obj)
{
    return std::visit(VisitorHelper{
        [](TArmor* p) -> TDslValue { return TDslValue(p); },
        [](TWeapon* p) -> TDslValue { return TDslValue(p); },
        [](const TWeapon* p) -> TDslValue { return TDslValue(p); },
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

bool DslEquals(const TDslValue& a, const TDslValue& b)
{
    if (a.data.index() != b.data.index()) {
        return false;
    }
    return std::visit(VisitorHelper{
        [&](std::monostate) { return true; },
        [&](bool x) { return x == b.AsBool(); },
        [&](int x) { return x == b.AsInt(); },
        [&](const TArmor* p) { return p == std::get<const TArmor*>(b.data); },
        [&](const TWeapon* p) { return p == std::get<const TWeapon*>(b.data); },
        [&](const TCreature* p) { return p == std::get<const TCreature*>(b.data); },
        [&](TPlayer* p) { return p == std::get<TPlayer*>(b.data); },
        [&](const TDslValue::TListPtr&) -> bool {
            throw std::runtime_error("dsl: list equality not supported");
        }
    }, a.data);
}

TDslValue Eval(const expr::TExprNode& node, TEvalContext& ctx);

TDslValue EvalVar(const expr::TExprNode& node, TEvalContext& ctx)
{
    auto scoped = ctx.scope.find(node.Text);
    if (scoped != ctx.scope.end()) {
        return scoped->second;
    }
    TGameObjectId id = TGameObjectIdManager::Instance().Register(node.Text);
    if (!ctx.registry->Contains(id)) {
        throw std::runtime_error("dsl: variable '$" + node.Text + "' not found");
    }
    return FromGameObject(ctx.registry->GetGameObjectPtr(id));
}

TDslValue EvalBinary(const expr::TExprNode& node, TEvalContext& ctx)
{
    using O = expr::EBinaryOp;
    if (node.BinOp == O::And) {
        if (!RequireBool(Eval(*node.Lhs, ctx), "&&")) {
            return TDslValue(false);
        }
        return TDslValue(RequireBool(Eval(*node.Rhs, ctx), "&&"));
    }
    if (node.BinOp == O::Or) {
        if (RequireBool(Eval(*node.Lhs, ctx), "||")) {
            return TDslValue(true);
        }
        return TDslValue(RequireBool(Eval(*node.Rhs, ctx), "||"));
    }

    TDslValue lhs = Eval(*node.Lhs, ctx);
    TDslValue rhs = Eval(*node.Rhs, ctx);
    switch (node.BinOp) {
        case O::Add: return TDslValue(RequireInt(lhs, "+") + RequireInt(rhs, "+"));
        case O::Sub: return TDslValue(RequireInt(lhs, "-") - RequireInt(rhs, "-"));
        case O::Mul: return TDslValue(RequireInt(lhs, "*") * RequireInt(rhs, "*"));
        case O::Div: {
            int divisor = RequireInt(rhs, "/");
            if (divisor == 0) {
                throw std::runtime_error("dsl: division by zero");
            }
            return TDslValue(RequireInt(lhs, "/") / divisor);
        }
        case O::Eq:  return TDslValue(DslEquals(lhs, rhs));
        case O::Ne:  return TDslValue(!DslEquals(lhs, rhs));
        case O::Lt:  return TDslValue(RequireInt(lhs, "<") < RequireInt(rhs, "<"));
        case O::Le:  return TDslValue(RequireInt(lhs, "<=") <= RequireInt(rhs, "<="));
        case O::Gt:  return TDslValue(RequireInt(lhs, ">") > RequireInt(rhs, ">"));
        case O::Ge:  return TDslValue(RequireInt(lhs, ">=") >= RequireInt(rhs, ">="));
        case O::And:
        case O::Or:
            break;
    }
    throw std::logic_error("dsl: unreachable binary op");
}

TDslValue Eval(const expr::TExprNode& node, TEvalContext& ctx)
{
    using K = expr::ENodeKind;
    switch (node.Kind) {
        case K::IntLiteral:
            return TDslValue(std::stoi(node.Text));
        case K::Var:
            return EvalVar(node, ctx);
        case K::Member:
            return GetDslProperty(Eval(*node.Lhs, ctx), node.Text, ctx);
        case K::Call: {
            std::vector<TDslValue> args;
            args.reserve(node.Args.size());
            for (const auto& a : node.Args) {
                args.push_back(Eval(a, ctx));
            }
            return TDslFunctionRegistry::Instance().Call(node.Text, args, ctx);
        }
        case K::Unary:
            return TDslValue(!RequireBool(Eval(*node.Lhs, ctx), "!"));
        case K::Binary:
            return EvalBinary(node, ctx);
    }
    throw std::logic_error("dsl: unreachable node kind");
}

}

TDslValue TDslExpression::Evaluate(TEvalContext& ctx) const
{
    return Eval(root_, ctx);
}
