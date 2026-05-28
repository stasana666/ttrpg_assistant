#include <pf2e_engine/initiative_order.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/transformation/transformator.h>

#include <iterator>

TInitiativeOrder::TInitiativeOrder(IRandomGenerator* dice_roller, IInteractionSystem& io_system)
    : dice_roller_(dice_roller)
    , current_(players_.end())
    , io_system_(io_system)
{
}

void TInitiativeOrder::AddPlayer(TPlayer* player)
{
    // TODO: спрашивать от какого навыка кидать инициативу
    int bonus = combat_calculator_.InitiativeBonus(*player->GetCreature());
    TInitiative initiative{
        .initiative = dice_roller_->RollDice(20) + bonus,
        .initiative_bonus = bonus,
    };

    io_system_.GameLog() << "Add player " << player->GetName() << " with initiative: " << initiative.initiative << std::endl;

    players_.emplace(initiative, player);
}

TPlayer* TInitiativeOrder::CurrentPlayer() const
{
    if (current_ == players_.end()) {
        return nullptr;
    }
    return current_->second;
}

void TInitiativeOrder::Next(TTransformator& transformator)
{
    if (current_ == players_.end()) {
        transformator.ChangeRound(this, round_ + 1);
        transformator.ChangeCurrentPlayer(this, 0);
    } else {
        transformator.ChangeCurrentPlayer(this, GetCurrentPosition() + 1);
    }
}

size_t TInitiativeOrder::CurrentRound() const
{
    return round_;
}

void TInitiativeOrder::SetCurrentPosition(size_t position)
{
    if (position == SIZE_MAX) {
        current_ = players_.end();
    } else {
        current_ = players_.begin();
        std::advance(current_, position);
    }
}

size_t TInitiativeOrder::GetCurrentPosition() const
{
    if (current_ == players_.end()) {
        return SIZE_MAX;
    }
    size_t position = 0;
    for (auto it = players_.begin(); it != current_; ++it) {
        ++position;
    }
    return position;
}

void TInitiativeOrder::SetRound(size_t round)
{
    round_ = round;
}

TAstNode TInitiativeOrder::GetAst(TAstContext& ctx) const
{
    static constexpr size_t kExpectedSize = 96;
    AST_ASSERT_LAYOUT(TInitiativeOrder, kExpectedSize);

    TAstNode node = TAstNode::MakeObject("TInitiativeOrder");
    AddValueField(node, "round", round_);
    AddValueField(node, "current_position", GetCurrentPosition());

    TAstNode order = TAstNode::MakeObject("players");
    size_t idx = 0;
    for (const auto& [init, player] : players_) {
        TAstNode entry = TAstNode::MakeObject("entry");
        AddValueField(entry, "initiative", init.initiative);
        AddValueField(entry, "initiative_bonus", init.initiative_bonus);
        AddReference(entry, "player", player, ctx);
        order.AddChild(std::to_string(idx++), std::move(entry));
    }
    node.AddChild("players", std::move(order));
    return node;
}
