#pragma once

#include <map>
#include <pf2e_engine/combat_calculator.h>

class IRandomGenerator;

class TInitiativeOrder {
public:
    explicit TInitiativeOrder(IRandomGenerator*);

    void AddPlayer(TPlayer* player);
    TPlayer* CurrentPlayer() const;
    size_t CurrentRound() const;
    void Next();

private:
    struct TInitiative {
        int initiative;
        int initiative_bonus;

        bool operator <(TInitiative other) const {
            if (initiative == other.initiative) {
                return initiative_bonus < other.initiative_bonus;
            }
            return initiative < other.initiative;
        }
    };

    using TContainer = std::multimap<TInitiative, TPlayer*>;
    using TIterator = TContainer::iterator;

    IRandomGenerator* dice_roller_;
    TContainer players_;
    TIterator current_;
    size_t round_{};
    TCombatCalculator combat_calculator_;
};
