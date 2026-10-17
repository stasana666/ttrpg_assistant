#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/transformation/state.h>
#include <pf2e_engine/transformation/transformation.h>
#include <pf2e_engine/i_interaction_system.h>
#include <pf2e_engine/creature.h>

class TTransformator {
public:
    explicit TTransformator(IInteractionSystem& io_system);

    void DealDamage(TPlayer* player, int damage);
    THealthDamageResult DealDamageWithContext(TPlayer* player, int damage,
                                             const TDamageContext& context);
    void Heal(TPlayer* player, int value);

    void ChangeCurrentPlayer(TInitiativeOrder* order, size_t new_position);
    void ChangeRound(TInitiativeOrder* order, size_t new_round);
    void MoveInitiativeBefore(TInitiativeOrder* order, TPlayer* target, TPlayer* anchor);

    void MovePlayer(TPlayer* player, TPosition new_position);

    void Undo(TState state);

    TState CurrentState() const;
    TGeneratedExecutionContext ExecutionContext();

    TAstNode GetAst(TAstContext& ctx) const;

private:
    friend class TGeneratedExecutionContext;

    IInteractionSystem& io_system_;
    std::vector<TTransformation> transformations_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TTransformator> : std::true_type {};
