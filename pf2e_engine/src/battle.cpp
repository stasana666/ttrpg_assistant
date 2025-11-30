#include <pf2e_engine/battle.h>

#include <pf2e_engine/alternatives.h>
#include <pf2e_engine/initiative_order.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/resources.h>
#include <pf2e_engine/transformation/transformator.h>
#include <pf2e_engine/scheduler.h>

#include <cassert>
#include <iostream>
#include <stdexcept>
#include "reaction.h"
#include "save_point.h"
#include <condition.h>

const TResourceId kActionId = TResourceIdManager::Instance().Register("action");

TBattle::TBattle(TBattleMap&& battle_map, IRandomGenerator* dice_roller, TInteractionSystem& io_system)
    : battle_map_(std::move(battle_map))
    , dice_roller_(dice_roller)
    , initiative_order_(dice_roller_, io_system)
    , io_system_(io_system)
    , transformator_(io_system)
{
}

void TBattle::AddPlayer(TPlayer&& player, TPosition position)
{
    players_.push_back(std::move(player));
    try {
        players_.back().BindWith(battle_map_, position);
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
    io_system_.GameLog() << "Start turn: player " << player.GetName() << std::endl;

    GiveStartResource(player);

    scheduler_.TriggerEvent(TEvent{.type = EEvent::OnTurnStart, .context = TEventContext{.player = &player}});
}

void TBattle::MakeTurn()
{
    TPlayer& player = *initiative_order_.CurrentPlayer();

    std::optional<TSavepointStackUnwind> last_save_point;
    TAction* action;
    while (!IsBattleEnd() && player.GetCreature()->IsAlive())
    {
        try {
            if (last_save_point.has_value()) {
                last_save_point->Revert(transformator_);
                last_save_point->Resume();
                last_save_point = std::nullopt;
            } else {
                if ((action = ChooseAction(player)) != nullptr) {
                    action->Apply(MakeActionContext(), player);
                } else {
                    break;
                }
            }
        } catch (TSavepointStackUnwind& save_point) {
            last_save_point = save_point;
        }
    }
}

void TBattle::EndTurn()
{
    assert(initiative_order_.CurrentPlayer() != nullptr);
    TPlayer& player = *initiative_order_.CurrentPlayer();

    initiative_order_.Next();

    io_system_.GameLog() << "End turn: player " << player.GetName() << std::endl;
    scheduler_.TriggerEvent(TEvent{.type = EEvent::OnTurnEnd, .context = TEventContext{.player = &player}});
}

bool TBattle::IsBattleEnd() const
{
    std::unordered_set<int> alive_teams;
    for (auto& player : players_) {
        if (player.GetCreature()->IsAlive()) {
            alive_teams.insert(player.GetTeam());
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
    assert(!player.GetCreature()->Resources().Count(kActionId));
    player.GetCreature()->Resources().Add(kActionId, 3);

    scheduler_.AddTask(TTask{
        .events_before_call = {TEvent{.type = EEvent::OnTurnEnd, .context = TEventContext{&player}}},
        .callback = [&player]() {
            size_t resource_count = player.GetCreature()->Resources().Count(kActionId);
            player.GetCreature()->Resources().Reduce(kActionId, resource_count);
            return false;
        },
    });
}

std::shared_ptr<TActionContext> TBattle::MakeActionContext()
{
    return std::make_shared<TActionContext>(TActionContext{
        .battle = this,
        .dice_roller = dice_roller_,
        .transformator = &transformator_,
        .io_system = &io_system_,
        .scheduler = &scheduler_,
        .effect_manager = &effect_manager_,
    });
}

TAction* TBattle::ChooseAction(TPlayer& player) const
{
    std::vector<TAction*> actions;
    for (auto& action : player.GetCreature()->Actions()) {
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

    return io_system_.ChooseAlternative(player.GetId(), alternatives);
}

std::shared_ptr<const TBattleMap> TBattle::BattleMap() const
{
    return battle_map_.Get();
}

THolder<TBattleMap>& TBattle::BattleMapMutable()
{
    return battle_map_;
}

std::vector<const TReaction*> TBattle::Reactions(ETrigger trigger) const
{
    std::vector<const TReaction*> reactions;
    for (auto& player : players_) {
        std::vector<const TReaction*> player_reactions = player.GetCreature()->Reactions(trigger);
        reactions.insert(reactions.begin(), player_reactions.begin(), player_reactions.end());
    }
    return reactions;
}
