#pragma once

#include <pf2e_engine/battle_map.h>
#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/holder.h>
#include <pf2e_engine/effect_manager.h>
#include <pf2e_engine/initiative_order.h>
#include <pf2e_engine/i_interaction_system.h>
#include <pf2e_engine/scheduler.h>
#include <pf2e_engine/transformation/transformator.h>
#include <pf2e_engine/actions/reaction.h>
#include <pf2e_engine/actions/ability.h>

#include <deque>
#include <optional>
#include <vector>

class TBattle {
public:
    explicit TBattle(TBattleMap&& battle_map, IRandomGenerator* dice_roller,
                     IInteractionSystem& io_system,
                     std::shared_ptr<const TAbilityRules> ability_rules = {});

    void StartBattle();
    void AddPlayer(TPlayer&& player, TPosition position, const TInitiativeOptions& options = {});
    const TPlayer* CurrentTurnPlayer() const;
    void MoveBeforeCurrentTurn(TPlayer& target, TTransformator& transformator);

    const TPlayer* GetPlayer(std::function<bool(const TPlayer*)> predicate) const;
    std::vector<TPlayer*> GetIfPlayers(std::function<bool(const TPlayer*)> predicate);
    std::vector<const TPlayer*> GetIfPlayers(std::function<bool(const TPlayer*)> predicate) const;

    std::vector<int> LivingTeams() const;
    std::optional<int> Winner() const;

    std::shared_ptr<const TBattleMap> BattleMap() const;
    THolder<TBattleMap>& BattleMapMutable();
    std::optional<TAbilityOption> ChooseAction(TPlayer&) const;

    std::vector<TAbilityOption> AvailableActions(const TPlayer&) const;
    std::vector<TAbilityOption> AvailableReactions(const TTriggerContext& trigger) const;
    EAbilityExecutionResult Execute(const TAbilityOption& option);
    const TAbilityRules& AbilityRules() const;

    TAstNode GetAst(TAstContext& ctx) const;

private:
    void StartRound();
    void EndRound();

    void StartTurn();
    void MakeTurn();
    void EndTurn();

    bool IsBattleEnd() const;
    bool IsRoundEnd() const;

    std::shared_ptr<TActionContext> MakeActionContext();

    THolder<TBattleMap> battle_map_;
    IRandomGenerator* dice_roller_;
    TInitiativeOrder initiative_order_;
    IInteractionSystem& io_system_;
    TTransformator transformator_;
    TTaskScheduler scheduler_;
    TEffectManager effect_manager_;
    std::shared_ptr<const TAbilityRules> ability_rules_;
    std::deque<TPlayer> players_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TBattle> : std::true_type {};
