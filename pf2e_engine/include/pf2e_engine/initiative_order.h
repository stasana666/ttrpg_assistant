#pragma once

#include <pf2e_engine/combat_calculator.h>
#include <pf2e_engine/i_interaction_system.h>

#include <map>

class IRandomGenerator;

class TTransformator;

class TInitiativeOrder {
public:
    TInitiativeOrder(IRandomGenerator*, IInteractionSystem& io_system);

    void AddPlayer(TPlayer* player);
    TPlayer* CurrentPlayer() const;
    size_t CurrentRound() const;
    void Next(TTransformator& transformator);

    // For transformation undo access
    void SetCurrentPosition(size_t position);  // SIZE_MAX means end()
    size_t GetCurrentPosition() const;
    void SetRound(size_t round);

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
    IInteractionSystem& io_system_;
};
