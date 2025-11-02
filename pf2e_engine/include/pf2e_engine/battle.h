#pragma once

#include <pf2e_engine/battle_map.h>
#include <pf2e_engine/initiative_order.h>
#include <pf2e_engine/interaction_system.h>
#include <pf2e_engine/transformation/transformator.h>

#include <deque>
#include "pf2e_engine/effect_manager.h"
#include "pf2e_engine/scheduler.h"

class TBattle {
public:
    explicit TBattle(TBattleMap&& battle_map, IRandomGenerator* dice_roller, TInteractionSystem& io_system);

    void StartBattle();
    void AddPlayer(TPlayer&& player);

    const TPlayer* GetPlayer(std::function<bool(const TPlayer*)> predicate) const;
    std::vector<TPlayer*> GetIfPlayers(std::function<bool(const TPlayer*)> predicate);

    const TBattleMap& BattleMap() const;
    TBattleMap& BattleMap();
    TAction* ChooseAction(TPlayer&) const;

private:
    void StartRound();
    void EndRound();

    void StartTurn();
    void MakeTurn();
    void EndTurn();

    bool IsBattleEnd() const;
    bool IsRoundEnd() const;

    void GiveStartResource(TPlayer&);

    TActionContext MakeActionContext();

    TBattleMap battle_map_;
    IRandomGenerator* dice_roller_;
    TInitiativeOrder initiative_order_;
    TInteractionSystem& io_system_;
    TTransformator transformator_;
    TTaskScheduler scheduler_;
    TEffectManager effect_manager_;
    std::deque<TPlayer> players_;
};
