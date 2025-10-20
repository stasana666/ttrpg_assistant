#include "dice_expression.h"

#include <pf2e_engine/game_context.h>

#include <cassert>

TDiceExpression::TDiceExpression(int size)
    : size(size)
{
}

int TDiceExpression::Value(TGameContext& ctx) const
{
    assert(ctx.dice_roller != nullptr);
    return ctx.dice_roller->RollDice(size);
}
