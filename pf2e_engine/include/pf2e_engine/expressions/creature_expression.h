#pragma once

#include "base_expression.h"

#include <pf2e_engine/mechanics/characteristics.h>

class TArmorClassExpression final : public IExpression {
public:
    explicit TArmorClassExpression(TGameObjectId creature_id);

    int Value(TGameContext& ctx) const final;

private:
    TGameObjectId creature_id_;
};

class TCharacteristicExpression final : public IExpression {
public:
    explicit TCharacteristicExpression(ECharacteristic type, TGameObjectId creature_id);

    int Value(TGameContext& ctx) const final;

private:
    ECharacteristic type_;
    TGameObjectId creature_id_;
};

class TWeaponBonusAttackExpression final : public IExpression {
public:
    explicit TWeaponBonusAttackExpression(TGameObjectId creature_id);

    int Value(TGameContext& ctx) const final;

private:
    TGameObjectId creature_id_;
};
