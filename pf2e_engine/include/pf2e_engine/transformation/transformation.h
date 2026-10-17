#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/position.h>
#include <variant>

class TInitiativeOrder;
class TPlayer;

class TChangeCurrentPlayer {
public:
    TChangeCurrentPlayer(TInitiativeOrder* order, size_t new_position);

    void Undo();

    TAstNode GetAst(TAstContext& ctx) const;

private:
    TInitiativeOrder* order_;
    size_t prev_position_;
};

class TChangeRound {
public:
    TChangeRound(TInitiativeOrder* order, size_t new_round);

    void Undo();

    TAstNode GetAst(TAstContext& ctx) const;

private:
    TInitiativeOrder* order_;
    size_t prev_round_;
};

class TMoveInitiativePlayer {
public:
    TMoveInitiativePlayer(TInitiativeOrder* order, TPlayer* target, TPlayer* anchor);

    void Undo();

    TAstNode GetAst(TAstContext& ctx) const;

private:
    TInitiativeOrder* order_;
    TPlayer* target_;
    TPlayer* previous_successor_;
};

class TMovePlayer {
public:
    TMovePlayer(TPlayer* player, TPosition new_position);

    void Undo();

    TAstNode GetAst(TAstContext& ctx) const;

private:
    TPlayer* player_;
    TPosition prev_position_;
};

using TTransformation = std::variant<
    TChangeCurrentPlayer,
    TChangeRound,
    TMoveInitiativePlayer,
    TMovePlayer,
    TGeneratedAssignment
>;
