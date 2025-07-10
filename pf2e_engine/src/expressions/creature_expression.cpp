#include "creature_expression.h"

#include <cassert>
#include "characteristics.h"
#include "game_object_storage.h"

TArmorClassExpression::TArmorClassExpression(TGameObjectId creatureId)
    : creatureId(creatureId)
{
}

int TArmorClassExpression::Value(TGameContext& ctx) const
{
    assert(ctx.gameObjectStorage != nullptr);
    TCreature* creature = ctx.gameObjectStorage->Get(creatureId);
    assert(creature != nullptr);

    const TArmor& armor = creature->GetArmor();
    return 10 + armor.AcBonus() + std::min(armor.DexCap(), creature->GetCharacteristic(ECharacteristic::Dexterity).GetMod());
}

TCharacteristicExpression::TCharacteristicExpression(ECharacteristic type, TGameObjectId creatureId)
    : type(type)
    , creatureId(creatureId)
{
}

int TCharacteristicExpression::Value(TGameContext& ctx) const
{
    assert(ctx.gameObjectStorage != nullptr);
    TCreature* creature = ctx.gameObjectStorage->Get(creatureId);
    assert(creature != nullptr);

    return creature->GetCharacteristic(type).GetMod();
}

TWeaponBonusAttackExpression::TWeaponBonusAttackExpression(TGameObjectId creatureId)
    : creatureId(creatureId)
{
}

int TWeaponBonusAttackExpression::Value(TGameContext& ctx) const
{
    assert(ctx.gameObjectStorage != nullptr);
    TCreature* creature = ctx.gameObjectStorage->Get(creatureId);
    assert(creature != nullptr);

    return creature->GetCharacteristic(ECharacteristic::Strength).GetMod();
}
