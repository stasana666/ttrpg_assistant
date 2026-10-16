#include <pf2e_engine/initiative_order.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>
#include <pf2e_engine/common/rule_integer.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/transformation/transformator.h>

#include <iterator>
#include <algorithm>
#include <limits>
#include <stdexcept>

TInitiativeOrder::TInitiativeOrder(IRandomGenerator* dice_roller, IInteractionSystem& io_system)
    : dice_roller_(dice_roller)
    , current_(players_.end())
    , io_system_(io_system)
{
}

void TInitiativeOrder::AddPlayer(TPlayer* player, const TInitiativeOptions& options)
{
    ValidateEnrollment(player);
    const auto& creature = *player->GetCreature();
    const int bonus = options.Skill
        ? creature.SkillBonusWithModifiers(*options.Skill, options.Modifiers)
        : combat_calculator_.InitiativeBonus(creature, options.Modifiers);
    const int natural = dice_roller_->RollDice(20);
    if (natural < 1 || natural > 20) {
        throw std::out_of_range("initiative roll must be between 1 and 20");
    }
    TInitiative initiative{
        .initiative = TRuleInteger::Add(natural, bonus),
        .initiative_bonus = bonus,
        .tie_order = options.TieOrder,
    };

    io_system_.GameLog() << "Add player " << player->GetName() << " with initiative: " << initiative.initiative << std::endl;

    const auto position = std::find_if(players_.begin(), players_.end(), [&](const auto& entry) {
        return initiative < entry.initiative;
    });
    players_.insert(position, TEntry{initiative, player});
}

void TInitiativeOrder::ValidateEnrollment(const TPlayer* player) const
{
    if (round_ != 0) {
        throw std::invalid_argument("initiative enrollment is closed after the first round starts");
    }
    if (player == nullptr || player->GetCreature() == nullptr) {
        throw std::invalid_argument("initiative requires a creature");
    }
    for (const auto& entry : players_) {
        if (entry.player == player || entry.player->GetId() == player->GetId()
            || entry.player->GetCreature()->Id() == player->GetCreature()->Id()) {
            throw std::invalid_argument("duplicate initiative participant identity");
        }
    }
}

TPlayer* TInitiativeOrder::CurrentPlayer() const
{
    if (current_ == players_.end()) {
        return nullptr;
    }
    return current_->player;
}

void TInitiativeOrder::Next(TTransformator& transformator)
{
    auto execution = transformator.ExecutionContext();
    auto transaction = execution.Atomic();
    if (current_ == players_.end()) {
        if (round_ == std::numeric_limits<size_t>::max()) {
            throw std::overflow_error("initiative round overflow");
        }
        transformator.ChangeRound(this, round_ + 1);
        transformator.ChangeCurrentPlayer(this, 0);
    } else {
        transformator.ChangeCurrentPlayer(this, GetCurrentPosition() + 1);
    }
    transaction.Commit();
}

void TInitiativeOrder::MoveBefore(TPlayer* target, TPlayer* anchor, TTransformator& transformator)
{
    transformator.MoveInitiativeBefore(this, target, anchor);
}

bool TInitiativeOrder::ValidateMoveBefore(const TPlayer* target, const TPlayer* anchor) const
{
    const auto find = [&](const TPlayer* player) {
        return std::find_if(players_.begin(), players_.end(), [player](const auto& entry) {
            return entry.player == player;
        });
    };
    const auto target_position = find(target);
    const auto anchor_position = find(anchor);
    if (target_position == players_.end() || anchor_position == players_.end()) {
        throw std::invalid_argument("initiative relocation requires enrolled participants");
    }
    return target_position != anchor_position && std::next(target_position) != anchor_position;
}

TPlayer* TInitiativeOrder::Successor(const TPlayer* target) const
{
    auto position = std::find_if(players_.begin(), players_.end(), [target](const auto& entry) {
        return entry.player == target;
    });
    if (position == players_.end()) {
        throw std::invalid_argument("initiative participant is not enrolled");
    }
    ++position;
    return position == players_.end() ? nullptr : position->player;
}

void TInitiativeOrder::Relocate(TPlayer* target, TPlayer* before)
{
    const auto find = [&](const TPlayer* player) {
        return std::find_if(players_.begin(), players_.end(), [player](const auto& entry) {
            return entry.player == player;
        });
    };
    const auto target_position = find(target);
    const auto before_position = find(before);
    if (target_position == players_.end() || (before != nullptr && before_position == players_.end())) {
        throw std::invalid_argument("initiative relocation requires enrolled participants");
    }
    players_.splice(before_position, players_, target_position);
}

size_t TInitiativeOrder::CurrentRound() const
{
    return round_;
}

void TInitiativeOrder::SetCurrentPosition(size_t position)
{
    if (position != SIZE_MAX && position > players_.size()) {
        throw std::out_of_range("initiative position is outside the order");
    }
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
    static constexpr size_t kExpectedSize = 72;
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
        AddValueField(entry, "tie_order", init.tie_order);
        AddReference(entry, "player", player, ctx);
        order.AddChild(std::to_string(idx++), std::move(entry));
    }
    node.AddChild("players", std::move(order));
    return node;
}
