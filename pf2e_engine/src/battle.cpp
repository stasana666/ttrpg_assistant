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

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>

#include <cassert>
#include <iostream>
#include <stdexcept>
#include "reaction.h"
#include "save_point.h"
#include <condition.h>

const TResourceId kActionId = TResourceIdManager::Instance().Register("action");

TBattle::TBattle(TBattleMap&& battle_map, IRandomGenerator* dice_roller, IInteractionSystem& io_system)
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
    io_system_.GameLog() << "End battle" << std::endl;
}

void TBattle::StartRound()
{
    assert(initiative_order_.CurrentPlayer() == nullptr);
    initiative_order_.Next(transformator_);
    io_system_.GameLog() << "Start round " << initiative_order_.CurrentRound() << " round" << std::endl;
}

void TBattle::EndRound()
{
    io_system_.GameLog() << "End round" << std::endl;
}

void TBattle::StartTurn()
{
    assert(initiative_order_.CurrentPlayer() != nullptr);
    TPlayer& player = *initiative_order_.CurrentPlayer();
    io_system_.GameLog() << "Start turn: player " << player.GetName() << std::endl;

    GiveStartResource(player);

    scheduler_.TriggerEvent(TEvent{.type = EEvent::OnTurnStart, .context = TEventContext{.player = &player}}, transformator_);
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

    initiative_order_.Next(transformator_);

    io_system_.GameLog() << "End turn: player " << player.GetName() << std::endl;
    scheduler_.TriggerEvent(TEvent{.type = EEvent::OnTurnEnd, .context = TEventContext{.player = &player}}, transformator_);
}

std::vector<int> TBattle::LivingTeams() const
{
    std::unordered_set<int> alive_teams;
    for (const auto& player : players_) {
        if (player.GetCreature()->IsAlive()) {
            alive_teams.insert(player.GetTeam());
        }
    }
    return {alive_teams.begin(), alive_teams.end()};
}

std::optional<int> TBattle::Winner() const
{
    std::vector<int> teams = LivingTeams();
    if (teams.size() == 1) {
        return teams.front();
    }
    return std::nullopt;
}

bool TBattle::IsBattleEnd() const
{
    return LivingTeams().size() < 2;
}

bool TBattle::IsRoundEnd() const
{
    return initiative_order_.CurrentPlayer() == nullptr;
}

void TBattle::GiveStartResource(TPlayer& player)
{
    assert(!player.GetCreature()->Resources().Count(kActionId));
    transformator_.AddResource(&player.GetCreature()->Resources(), kActionId, 3);

    transformator_.AddTask(&scheduler_, TTask{
        .events_before_call = {TEvent{.type = EEvent::OnTurnEnd, .context = TEventContext{&player}}},
        .callback = [&player, this]() {
            int resource_count = static_cast<int>(player.GetCreature()->Resources().Count(kActionId));
            transformator_.ReduceResource(&player.GetCreature()->Resources(), kActionId, resource_count);
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

    TAlternatives alternatives = TAlternatives::Create<TAction*>("next action");

    alternatives.AddAlternative("End of turn", static_cast<TAction*>(nullptr));
    for (auto& action : actions) {
        alternatives.AddAlternative(std::string(action->Name()), action);
    }

    return io_system_.ChooseAlternative<TAction*>(player.GetId(), alternatives);
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

TAstNode TBattle::GetAst(TAstContext& ctx) const
{
    // TBattle is non-standard-layout (holds an IInteractionSystem& reference),
    // so offsetof on the sentinel is UB. Rely on sizeof alone here; a field
    // that exactly fills trailing padding will not be caught for TBattle
    // specifically.
    static constexpr size_t kExpectedSize = 472;
    AST_ASSERT_LAYOUT(TBattle, kExpectedSize);

    // Register only the identities this class owns the names for. Each child
    // class self-registers its own internal sub-objects (TPlayer registers
    // its creature, TCreature registers its hitpoints/resources, etc.).
    // Order does not matter: AddReference emits a deferred placeholder and
    // node.Resolve(ctx) at the end looks every placeholder up after the whole
    // graph has been walked.
    ctx.RegisterIdentity(&initiative_order_, "battle.initiative_order");
    ctx.RegisterIdentity(&scheduler_,        "battle.scheduler");
    ctx.RegisterIdentity(&effect_manager_,   "battle.effect_manager");
    ctx.RegisterIdentity(&transformator_,    "battle.transformator");
    ctx.RegisterIdentity(&battle_map_,       "battle.map_holder");
    for (const auto& player : players_) {
        ctx.RegisterIdentity(&player, "player#" + std::to_string(player.GetId()));
    }

    TAstNode node = TAstNode::MakeObject("TBattle");
    AddOwnedObject(node, "battle_map", battle_map_, ctx);  // THolder::GetAst locks internally
    AddOwnedObject(node, "initiative_order", initiative_order_, ctx);
    AddOwnedObject(node, "transformator", transformator_, ctx);
    AddOwnedObject(node, "scheduler", scheduler_, ctx);
    AddOwnedObject(node, "effect_manager", effect_manager_, ctx);

    TAstNode players_node = TAstNode::MakeObject("players");
    for (const auto& player : players_) {
        AddOwnedObject(players_node, "player#" + std::to_string(player.GetId()),
                       player, ctx);
    }
    node.AddChild("players", std::move(players_node));

    // Replace every deferred reference placeholder with the resolved stable id.
    node.Resolve(ctx);
    return node;
}

// Mutable accessor for THitPoints (used by TChangeHitPoints) — needed only
// after the AST-related includes pulled in non-const requirements; kept here
// as a tiny non-mutating addition. (This stub does nothing — actual mutations
// happen via TTransformator.)
