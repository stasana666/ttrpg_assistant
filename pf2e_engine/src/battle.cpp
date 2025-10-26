#include <pf2e_engine/battle.h>
#include <pf2e_engine/initiative_order.h>
#include <cassert>
#include <iostream>
#include <stdexcept>

TBattle::TBattle(TBattleMap&& battle_map, IRandomGenerator* dice_roller)
    : battle_map_(std::move(battle_map))
    , dice_roller_(dice_roller)
    , initiative_order_(dice_roller_)
{
}

void TBattle::AddPlayer(TPlayer&& player)
{
    players_.push_back(std::move(player));
    try {
        battle_map_.AddPlayer(&players_.back());
        initiative_order_.AddPlayer(&players_.back());
    } catch (std::runtime_error&) {
        players_.pop_back();
    }
}

std::vector<TPlayer*> TBattle::GetIfPlayers(std::function<bool(const TPlayer*)> predicate)
{
    std::vector<TPlayer*> result;
    for (auto& player : players_) {
        if (predicate(&player)) {
            result.emplace_back(&player);
        }
    }
    return result;
}

const TPlayer* TBattle::GetPlayer(std::function<bool(const TPlayer*)> predicate) const
{
    for (auto& player : players_) {
        if (predicate(&player)) {
            return &player;
        }
    }
    return nullptr;
}

void TBattle::StartBattle()
{
    while (!IsBattleEnd()) {
        StartRound();
        while (initiative_order_.CurrentPlayer() != nullptr) {
            StartTurn();
            initiative_order_.Next();
        }
    }
}

void TBattle::StartRound()
{
    assert(initiative_order_.CurrentPlayer() == nullptr);
    initiative_order_.Next();
    std::cout << "Start round " << initiative_order_.CurrentRound() << "th round" << std::endl;
}

void TBattle::StartTurn()
{
    assert(initiative_order_.CurrentPlayer() != nullptr);
    TPlayer* player = initiative_order_.CurrentPlayer();
    std::cout << "Start turn: player's name: " << player->name << std::endl;
    player->creature->Hitpoints().ReduceHp(dice_roller_->RollDice(6));
    std::cout << "\t" << "current hp: " << player->creature->Hitpoints().GetCurrentHp() << std::endl;
}

bool TBattle::IsBattleEnd() const
{
    std::unordered_set<int> alive_teams;
    for (auto& player : players_) {
        if (player.creature->IsAlive()) {
            alive_teams.insert(player.team);
        }
    }
    return alive_teams.size() < 2;
}

const TBattleMap& TBattle::BattleMap() const
{
    return battle_map_;
}

TBattleMap& TBattle::BattleMap()
{
    return battle_map_;
}
