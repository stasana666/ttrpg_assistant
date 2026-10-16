#pragma once

#include <pf2e_engine/combat_calculator.h>
#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/i_interaction_system.h>
#include <pf2e_engine/inventory/check_modifiers.h>

#include <list>
#include <optional>

class IRandomGenerator;

class TTransformator;

struct TInitiativeOptions {
    std::optional<ESkill> Skill = {};
    TCheckModifiers Modifiers = TCheckModifiers::Create({});
    int TieOrder = 0;
};

class TInitiativeOrder {
public:
    TInitiativeOrder(IRandomGenerator*, IInteractionSystem& io_system);

    void AddPlayer(TPlayer* player, const TInitiativeOptions& options = {});
    void ValidateEnrollment(const TPlayer* player) const;
    TPlayer* CurrentPlayer() const;
    size_t CurrentRound() const;
    void Next(TTransformator& transformator);
    void MoveBefore(TPlayer* target, TPlayer* anchor, TTransformator& transformator);

    void SetCurrentPosition(size_t position);
    size_t GetCurrentPosition() const;
    void SetRound(size_t round);

    TAstNode GetAst(TAstContext& ctx) const;

private:
    friend class TMoveInitiativePlayer;
    friend class TTransformator;

    struct TInitiative {
        int initiative;
        int initiative_bonus;
        int tie_order;

        bool operator <(const TInitiative& other) const {
            if (initiative == other.initiative) {
                return tie_order < other.tie_order;
            }
            return initiative > other.initiative;
        }
    };

    struct TEntry {
        TInitiative initiative;
        TPlayer* player;
    };

    using TContainer = std::list<TEntry>;
    using TIterator = TContainer::iterator;

    bool ValidateMoveBefore(const TPlayer* target, const TPlayer* anchor) const;
    TPlayer* Successor(const TPlayer* target) const;
    void Relocate(TPlayer* target, TPlayer* before);

    IRandomGenerator* dice_roller_;
    TContainer players_;
    TIterator current_;
    size_t round_{};
    TCombatCalculator combat_calculator_;
    IInteractionSystem& io_system_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TInitiativeOrder> : std::true_type {};
