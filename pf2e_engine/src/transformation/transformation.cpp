#include <pf2e_engine/transformation/transformation.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/initiative_order.h>
#include <pf2e_engine/player.h>

#include <pf2e_engine/common/ast/ast_helpers.h>

#include <stdexcept>

TChangeCurrentPlayer::TChangeCurrentPlayer(TInitiativeOrder* order, size_t new_position)
    : order_(order)
    , prev_position_(order_->GetCurrentPosition())
{
    order_->SetCurrentPosition(new_position);
}

void TChangeCurrentPlayer::Undo()
{
    order_->SetCurrentPosition(prev_position_);
}

TChangeRound::TChangeRound(TInitiativeOrder* order, size_t new_round)
    : order_(order)
    , prev_round_(order_->CurrentRound())
{
    order_->SetRound(new_round);
}

void TChangeRound::Undo()
{
    order_->SetRound(prev_round_);
}

TMoveInitiativePlayer::TMoveInitiativePlayer(TInitiativeOrder* order, TPlayer* target, TPlayer* anchor)
    : order_(order)
    , target_(target)
    , previous_successor_(nullptr)
{
    if (order_ == nullptr) {
        throw std::invalid_argument("initiative relocation requires an order");
    }
    order_->ValidateMoveBefore(target, anchor);
    previous_successor_ = order_->Successor(target);
    order_->Relocate(target, anchor);
}

void TMoveInitiativePlayer::Undo()
{
    order_->Relocate(target_, previous_successor_);
}

TMovePlayer::TMovePlayer(TPlayer* player, TPosition new_position)
    : player_(player)
    , prev_position_{}
{
    if (player_ == nullptr) {
        throw std::invalid_argument("movement requires a player");
    }
    prev_position_ = player_->GetPosition();
    player_->SetPosition(new_position);
}

void TMovePlayer::Undo()
{
    player_->SetPosition(prev_position_);
}

TAstNode TChangeCurrentPlayer::GetAst(TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("TChangeCurrentPlayer");
    AddReference(node, "order_ref", order_, ctx);
    AddValueField(node, "prev_position", prev_position_);
    return node;
}

TAstNode TChangeRound::GetAst(TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("TChangeRound");
    AddReference(node, "order_ref", order_, ctx);
    AddValueField(node, "prev_round", prev_round_);
    return node;
}

TAstNode TMovePlayer::GetAst(TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("TMovePlayer");
    AddReference(node, "player_ref", player_, ctx);
    AddValueField(node, "prev_position", prev_position_);
    return node;
}

TAstNode TMoveInitiativePlayer::GetAst(TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("TMoveInitiativePlayer");
    AddReference(node, "order_ref", order_, ctx);
    AddReference(node, "target_ref", target_, ctx);
    AddReference(node, "previous_successor_ref", previous_successor_, ctx);
    return node;
}
