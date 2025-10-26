#include <pf2e_engine/initiative_order.h>

TInitiativeOrder::TInitiativeOrder(IRandomGenerator* dice_roller)
    : dice_roller_(dice_roller)
    , current_(players_.end())
{
}

void TInitiativeOrder::AddPlayer(TPlayer* player)
{
    // TODO: спрашивать от какого навыка кидать инициативу
    int bonus = combat_calculator_.InitiativeBonus(*player->creature);
    TInitiative initiative{
        .initiative = dice_roller_->RollDice(20) + bonus,
        .initiative_bonus = bonus,
    };

    players_.emplace(initiative, player);
}

TPlayer* TInitiativeOrder::CurrentPlayer() const
{
    if (current_ == players_.end()) {
        return nullptr;
    }
    return current_->second;
}

void TInitiativeOrder::Next()
{
    if (current_ == players_.end()) {
        ++round_;
        current_ = players_.begin();
    } else {
        ++current_;
    }
}

size_t TInitiativeOrder::CurrentRound() const
{
    return round_;
}
