#include <pf2e_engine/battle.h>

#include <pf2e_engine/alternatives.h>
#include <pf2e_engine/initiative_order.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/transformation/transformator.h>
#include <pf2e_engine/scheduler.h>
#include <pf2e_engine/mechanics/creature.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>

#include <cassert>
#include <algorithm>
#include <map>
#include <utility>
#include <iostream>
#include <stdexcept>
#include "reaction.h"
#include "save_point.h"
#include <pf2e_engine/condition.h>
#include <pf2e_engine/actions/generated_world.h>

namespace {
constexpr int kActionsPerTurn = 3;

const TWeapon* SourceWeapon(const TCreature& creature, const TAbilitySourcePath& source)
{
    for (const auto& [field, weapons] : {
            std::pair{"weapons", &creature.Weapons()},
            std::pair{"natural_weapons", &creature.NaturalWeapons()}}) {
        for (auto entry : *weapons) {
            if (source.StartsWith(TAbilitySourcePath{}.Field(field).Entry(entry.Id().Value))) {
                return &*entry;
            }
        }
    }
    return nullptr;
}

std::string SourceName(const TCreature& creature, const TAbilitySourcePath& source)
{
    if (const auto* weapon = SourceWeapon(creature, source)) {
        return weapon->Name();
    }
    if (source.StartsWith(TAbilitySourcePath{}.Field("race"))) {
        return "Ancestry feature";
    }
    if (source.StartsWith(TAbilitySourcePath{}.Field("class"))) {
        return "Class feature";
    }
    return "Creature feature";
}
}

TBattle::TBattle(TBattleMap&& battle_map, IRandomGenerator* dice_roller,
                 IInteractionSystem& io_system, std::shared_ptr<const TAbilityRules> ability_rules)
    : battle_map_(std::move(battle_map))
    , dice_roller_(dice_roller)
    , initiative_order_(dice_roller_, io_system)
    , io_system_(io_system)
    , transformator_(io_system)
    , ability_rules_(ability_rules ? std::move(ability_rules) : std::make_shared<const TAbilityRules>())
{
}

void TBattle::AddPlayer(TPlayer&& player, TPosition position)
{
    const auto* creature = player.GetCreature();
    if (creature == nullptr || creature->Id().Value == 0) {
        throw std::invalid_argument("player must reference a creature with an identity");
    }
    for (const auto& existing : players_) {
        if (existing.GetId() == player.GetId() || existing.GetCreature()->Id() == creature->Id()) {
            throw std::invalid_argument("duplicate player or creature identity in battle");
        }
    }
    const auto abilities = CollectAbilities(*creature, ability_rules_->Catalog);
    for (const auto& grant : abilities.Grants()) {
        ability_rules_->Executions.Require(grant.DefinitionId);
    }
    auto original_map = battle_map_.Copy();
    players_.push_back(std::move(player));
    try {
        players_.back().BindWith(battle_map_, position);
        initiative_order_.AddPlayer(&players_.back());
    } catch (...) {
        battle_map_.Set(std::move(original_map));
        players_.pop_back();
        throw;
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

std::vector<const TPlayer*> TBattle::GetIfPlayers(
    std::function<bool(const TPlayer*)> predicate) const
{
    std::vector<const TPlayer*> result;
    for (const auto& player : players_) {
        if (predicate(&player)) {
            result.push_back(&player);
        }
    }
    return result;
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

    auto execution = transformator_.ExecutionContext();
    execution.Invoke(TGuarded<TCreature>(*player.GetCreature()), &TCreature::ResetAttacks);
    GiveStartResource(player);

    scheduler_.TriggerEvent(TEvent{.type = EEvent::OnTurnStart, .context = TEventContext{.player = &player}}, transformator_);
}

void TBattle::MakeTurn()
{
    TPlayer& player = *initiative_order_.CurrentPlayer();

    std::optional<TSavepointStackUnwind> last_save_point;
    while (!IsBattleEnd() && player.GetCreature()->IsAlive())
    {
        try {
            if (last_save_point.has_value()) {
                last_save_point->Revert(transformator_);
                last_save_point->Resume();
                last_save_point = std::nullopt;
            } else {
                if (auto action = ChooseAction(player)) {
                    Execute(*action);
                } else {
                    break;
                }
            }
        } catch (TSavepointStackUnwind& save_point) {
            last_save_point = save_point;
        } catch (const TAbilityUnavailable&) {
            last_save_point.reset();
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
    auto execution = transformator_.ExecutionContext();
    execution.Invoke(TGuarded<TCreature>(*player.GetCreature()), &TCreature::ResetAttacks);
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
    assert(!ResourceFor(*player.GetCreature(), EResourceKind::Action)->Count());
    transformator_.AddResource(ResourceFor(*player.GetCreature(), EResourceKind::Action),
                               kActionsPerTurn);

    transformator_.AddTask(&scheduler_, TTask{
        .events_before_call = {TEvent{.type = EEvent::OnTurnEnd, .context = TEventContext{&player}}},
        .callback = [&player, this]() {
            int resource_count =
                ResourceFor(*player.GetCreature(), EResourceKind::Action)->Count();
            transformator_.ReduceResource(
                ResourceFor(*player.GetCreature(), EResourceKind::Action), resource_count);
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

std::vector<TAbilityOption> TBattle::AvailableActions(const TPlayer& player) const
{
    const auto& creature = *player.GetCreature();
    if (!creature.IsAlive()) {
        return {};
    }
    const TGeneratedWorldView world(this);
    return EnumerateActions(player.GetId(), CollectAbilities(creature, ability_rules_->Catalog),
                            ability_rules_->Executions, player, &world);
}

std::optional<TAbilityOption> TBattle::ChooseAction(TPlayer& player) const
{
    const auto actions = AvailableActions(player);
    if (actions.empty()) {
        return std::nullopt;
    }

    auto alternatives = TAlternatives::Create<std::optional<TAbilityOption>>("next action");
    alternatives.AddAlternative("End of turn", std::optional<TAbilityOption>{});
    std::map<std::string, size_t> labels;
    for (const auto& action : actions) {
        std::string name(action.Action->Name());
        if (std::ranges::count_if(actions, [&](const TAbilityOption& other) {
                return other.Action->Name() == action.Action->Name();
            }) > 1) {
            name += " (" + SourceName(*std::as_const(player).GetCreature(), action.Source) + ")";
            const auto occurrence = ++labels[name];
            if (occurrence > 1) {
                name += " " + std::to_string(occurrence);
            }
        }
        alternatives.AddAlternative(std::move(name), std::optional<TAbilityOption>{action});
    }
    return io_system_.ChooseAlternative<std::optional<TAbilityOption>>(player.GetId(), alternatives);
}

EAbilityExecutionResult TBattle::Execute(const TAbilityOption& option)
{
    auto actors = GetIfPlayers([&](const TPlayer* player) { return player->GetId() == option.ActorId; });
    if (actors.size() != 1) {
        return EAbilityExecutionResult::Unavailable;
    }
    TPlayer& actor = *actors.front();
    const auto& creature = *std::as_const(actor).GetCreature();
    if (!creature.IsAlive()) {
        return EAbilityExecutionResult::Unavailable;
    }
    auto fresh = CollectAbilities(creature, ability_rules_->Catalog);
    TAbilityExecutionBindings bindings;
    if (const auto* weapon = SourceWeapon(creature, option.Source)) {
        bindings.SourceWeapon = std::make_shared<const TWeapon>(*weapon);
    }
    if (option.Trigger) {
        auto triggered = GetIfPlayers([&](const TPlayer* player) {
            return player->GetId() == option.Trigger->ActorId;
        });
        if (triggered.size() != 1) {
            return EAbilityExecutionResult::Unavailable;
        }
        bindings.TriggerActor = triggered.front();
    }
    bindings.ValidateSource = [&creature, rules = ability_rules_,
                              definition = option.DefinitionId, source = option.Source]() {
        const auto grants = CollectAbilities(creature, rules->Catalog);
        return std::ranges::any_of(grants.Grants(), [&](const TGrantedAbility& grant) {
            return grant.DefinitionId == definition && grant.Source == source;
        });
    };
    return ExecuteAbility(option, MakeActionContext(), actor, fresh,
                          ability_rules_->Executions, std::move(bindings));
}

const TAbilityRules& TBattle::AbilityRules() const
{
    return *ability_rules_;
}

std::shared_ptr<const TBattleMap> TBattle::BattleMap() const
{
    return battle_map_.Get();
}

THolder<TBattleMap>& TBattle::BattleMapMutable()
{
    return battle_map_;
}

std::vector<TAbilityOption> TBattle::AvailableReactions(const TTriggerContext& trigger) const
{
    std::vector<TAbilityOption> reactions;
    const TGeneratedWorldView world(this);
    for (const auto& player : players_) {
        const auto& creature = *player.GetCreature();
        if (!creature.IsAlive()) {
            continue;
        }
        auto candidates = EnumerateReactions(
            player.GetId(), CollectAbilities(creature, ability_rules_->Catalog),
            ability_rules_->Executions, player, trigger, &world);
        reactions.insert(reactions.end(), candidates.begin(), candidates.end());
    }
    return reactions;
}

TAstNode TBattle::GetAst(TAstContext& ctx) const
{
    static constexpr size_t kExpectedSize = 488;
    AST_ASSERT_LAYOUT(TBattle, kExpectedSize);

    ctx.RegisterIdentity(&initiative_order_, "battle.initiative_order");
    ctx.RegisterIdentity(&scheduler_,        "battle.scheduler");
    ctx.RegisterIdentity(&effect_manager_,   "battle.effect_manager");
    ctx.RegisterIdentity(&transformator_,    "battle.transformator");
    ctx.RegisterIdentity(&battle_map_,       "battle.map_holder");
    for (const auto& player : players_) {
        ctx.RegisterIdentity(&player, "player#" + std::to_string(player.GetId()));
    }

    TAstNode node = TAstNode::MakeObject("TBattle");
    AddReference(node, "ability_rules", ability_rules_.get(), "battle.ability_rules");
    AddOwnedObject(node, "battle_map", battle_map_, ctx);
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
    return node;
}
