#include "dice_expression.h"

#include <pf2e_engine/game_context.h>

#include <cassert>

TDiceExpression::TDiceExpression(int size)
    : size(size)
{
}

int TDiceExpression::Value(TGameContext& ctx) const
{
    assert(ctx.diceRoller != nullptr);
    return ctx.diceRoller->RollDice(size);
}
