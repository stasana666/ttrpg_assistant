#include <pf2e_engine/battle.h>

#include <pf2e_engine/alternatives.h>
#include <pf2e_engine/initiative_order.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/resources.h>
#include <pf2e_engine/transformation/transformator.h>

#include <cassert>
#include <iostream>
#include <stdexcept>

const TResourceId kActionId = TResourceIdManager::Instance().Register("action");

TBattle::TBattle(TBattleMap&& battle_map, IRandomGenerator* dice_roller, TInteractionSystem& io_system)
    : battle_map_(std::move(battle_map))
    , dice_roller_(dice_roller)
    , initiative_order_(dice_roller_, io_system)
    , io_system_(io_system)
    , transformator_(io_system)
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
        while (!IsRoundEnd()) {
            StartTurn();
            MakeTurn();
            EndTurn();
        }
        EndRound();
    }
}

void TBattle::StartRound()
{
    assert(initiative_order_.CurrentPlayer() == nullptr);
    initiative_order_.Next();
    std::cout << "Start round " << initiative_order_.CurrentRound() << " round" << std::endl;
}

void TBattle::EndRound()
{
}

void TBattle::StartTurn()
{
    assert(initiative_order_.CurrentPlayer() != nullptr);
    TPlayer& player = *initiative_order_.CurrentPlayer();
    io_system_.GameLog() << "Start turn: player " << player.name << std::endl;
    GiveStartResource(player);
}

void TBattle::MakeTurn()
{
    TPlayer& player = *initiative_order_.CurrentPlayer();

    TAction* action;
    while (!IsBattleEnd() && player.creature->IsAlive() && (action = ChooseAction(player)) != nullptr)
    {
        action->Apply(MakeActionContext(), player);
    }
}

void TBattle::EndTurn()
{
    assert(initiative_order_.CurrentPlayer() != nullptr);
    TPlayer& player = *initiative_order_.CurrentPlayer();

    size_t resource_count = player.creature->Resources().Count(kActionId);
    player.creature->Resources().Reduce(kActionId, resource_count);

    initiative_order_.Next();

    io_system_.GameLog() << "End turn: player " << player.name << std::endl;
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

bool TBattle::IsRoundEnd() const
{
    return initiative_order_.CurrentPlayer() == nullptr;
}

void TBattle::GiveStartResource(TPlayer& player)
{
    assert(!player.creature->Resources().Count(kActionId));
    player.creature->Resources().Add(kActionId, 3);
}

TActionContext TBattle::MakeActionContext()
{
    return {
        .battle = this,
        .dice_roller = dice_roller_,
        .transformator = &transformator_,
        .io_system = &io_system_,
    };
}

TAction* TBattle::ChooseAction(TPlayer& player) const
{
    std::vector<TAction*> actions;
    for (auto& action : player.creature->Actions()) {
        if (action->Check(player)) {
            actions.emplace_back(&*action);
        }
    }

    if (actions.empty()) {
        return nullptr;
    }

    TAlternatives<TAction*> alternatives("next action");

    alternatives.AddAlternative(TAlternative<TAction*>{
        .name = "End of turn",
        .value = nullptr
    });
    for (auto& action : actions) {
        alternatives.AddAlternative(TAlternative<TAction*>{
            .name = std::string(action->Name()),
            .value = action
        });
    }

    return io_system_.ChooseAlternative(player.id, alternatives);
}

const TBattleMap& TBattle::BattleMap() const
{
    return battle_map_;
}

TBattleMap& TBattle::BattleMap()
{
    return battle_map_;
}
