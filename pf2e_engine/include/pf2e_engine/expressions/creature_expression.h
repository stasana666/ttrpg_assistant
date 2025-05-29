#pragma once

#include "base_expression.h"

#include <pf2e_engine/mechanics/characteristics.h>

class TArmorClassExpression final : public IExpression {
public:
    explicit TArmorClassExpression(TGameObjectId creatureId);

    int Value(TGameContext& ctx) const final;

private:
    TGameObjectId creatureId;
};

class TCharacteristicExpression final : public IExpression {
public:
    explicit TCharacteristicExpression(ECharacteristic type, TGameObjectId creatureId);

    int Value(TGameContext& ctx) const final;

private:
    ECharacteristic type;
    TGameObjectId creatureId;
};
