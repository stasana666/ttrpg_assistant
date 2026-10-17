#include <pf2e_engine/transformation/transformator.h>
#include <pf2e_engine/common/visit.h>
#include <pf2e_engine/i_interaction_system.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/initiative_order.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>

#include <stdexcept>

TState::TState(size_t stack_size, const TTransformator* owner)
    : stack_size_(stack_size)
    , owner_(owner)
{
}

TTransformator::TTransformator(IInteractionSystem& io_system)
    : io_system_(io_system)
{
}

void TTransformator::DealDamage(TPlayer* player, int damage)
{
    DealDamageWithContext(player, damage, TDamageContext::Create({}));
}

THealthDamageResult TTransformator::DealDamageWithContext(TPlayer* player, int damage,
                                                        const TDamageContext& context)
{
    if (player == nullptr || player->GetCreature() == nullptr) {
        throw std::invalid_argument("damage requires a creature participant");
    }
    TCreature* creature = player->GetCreature();
    auto ctx = ExecutionContext();
    auto result = ctx.Invoke(TGuarded<TCreature>(*creature), &TCreature::ApplyResolvedDamage,
                             damage, context);
    io_system_.GameLog() << player->GetName() << " takes " << damage << " amount of damage" << std::endl;
    io_system_.GameLog() << "current hp: " << creature->Hitpoints()->Current() << std::endl;
    return result;
}

void TTransformator::Heal(TPlayer* player, int value)
{
    if (player == nullptr || player->GetCreature() == nullptr) {
        throw std::invalid_argument("healing requires a creature participant");
    }
    TCreature* creature = player->GetCreature();
    auto ctx = ExecutionContext();
    ctx.Invoke(TGuarded<TCreature>(*creature), &TCreature::ReceiveHealing, value);
    io_system_.GameLog() << player->GetName() << " takes " << value << " amount of heal" << std::endl;
    io_system_.GameLog() << "current hp: " << creature->Hitpoints()->Current() << std::endl;
}

void TTransformator::ChangeCurrentPlayer(TInitiativeOrder* order, size_t new_position)
{
    transformations_.emplace_back(TChangeCurrentPlayer(order, new_position));
}

void TTransformator::ChangeRound(TInitiativeOrder* order, size_t new_round)
{
    transformations_.emplace_back(TChangeRound(order, new_round));
}

void TTransformator::MovePlayer(TPlayer* player, TPosition new_position)
{
    static_assert(std::is_nothrow_move_constructible_v<TTransformation>);
    transformations_.emplace_back(std::in_place_type<TMovePlayer>, player, new_position);
}

void TTransformator::MoveInitiativeBefore(TInitiativeOrder* order, TPlayer* target, TPlayer* anchor)
{
    if (order == nullptr) {
        throw std::invalid_argument("initiative relocation requires an order");
    }
    if (!order->ValidateMoveBefore(target, anchor)) {
        return;
    }
    static_assert(std::is_nothrow_move_constructible_v<TTransformation>);
    transformations_.emplace_back(std::in_place_type<TMoveInitiativePlayer>, order, target, anchor);
}

void TTransformator::Undo(TState state)
{
    if (state.owner_ != this || state.stack_size_ > transformations_.size()) {
        throw std::invalid_argument("checkpoint does not belong to the current transformation history");
    }
    while (transformations_.size() > state.stack_size_) {
        std::visit(VisitorHelper{
            [&](auto& v) {
                v.Undo();
            }
        }, transformations_.back());
        transformations_.pop_back();
    }
}

TState TTransformator::CurrentState() const
{
    return TState{transformations_.size(), this};
}

TGeneratedExecutionContext TTransformator::ExecutionContext()
{
    return TGeneratedExecutionContext(*this);
}

TAstNode TTransformator::GetAst(TAstContext& ctx) const
{
    static constexpr size_t kExpectedSize = 40;
    AST_ASSERT_LAYOUT(TTransformator, kExpectedSize);

    TAstNode node = TAstNode::MakeObject("TTransformator");
    AddValueField(node, "depth", transformations_.size());

    TAstNode stack = TAstNode::MakeObject("transformations");
    for (size_t i = 0; i < transformations_.size(); ++i) {
        std::visit(
            [&](const auto& t) {
                stack.AddChild(std::to_string(i), t.GetAst(ctx));
            },
            transformations_[i]);
    }
    node.AddChild("transformations", std::move(stack));
    return node;
}
