#pragma once

#include <pf2e_engine/battle_map.h>
#include <pf2e_engine/initiative_order.h>

#include <deque>

class TBattle {
public:
    explicit TBattle(TBattleMap&& battle_map, IRandomGenerator* dice_roller);

    void StartBattle();
    void AddPlayer(TPlayer&& player);

    const TPlayer* GetPlayer(std::function<bool(const TPlayer*)> predicate) const;
    std::vector<TPlayer*> GetIfPlayers(std::function<bool(const TPlayer*)> predicate);

    const TBattleMap& BattleMap() const;
    TBattleMap& BattleMap();
    TAction* ChooseAction(TPlayer&) const;

private:
    void StartTurn();
    void EndTurn();
    void StartRound();
    bool IsBattleEnd() const;

    void GiveStartResource(TPlayer&);

    TBattleMap battle_map_;
    IRandomGenerator* dice_roller_;
    TInitiativeOrder initiative_order_;
    std::deque<TPlayer> players_;
};
