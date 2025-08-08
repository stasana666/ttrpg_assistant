#include "creature_expression.h"

#include <cassert>
#include "characteristics.h"
#include <pf2e_engine/game_object_logic/game_object_storage.h>

TArmorClassExpression::TArmorClassExpression(TGameObjectId creature_id)
    : creature_id_(creature_id)
{
}

int TArmorClassExpression::Value(TGameContext& ctx) const
{
    assert(ctx.gameObjectStorage != nullptr);
    TCreature* creature = ctx.gameObjectStorage->Get(creature_id_);
    assert(creature != nullptr);

    const TArmor& armor = creature->GetArmor();
    return 10 + armor.AcBonus() + std::min(armor.DexCap(), creature->GetCharacteristic(ECharacteristic::Dexterity).GetMod());
}

TCharacteristicExpression::TCharacteristicExpression(ECharacteristic type, TGameObjectId creature_id)
    : type_(type)
    , creature_id_(creature_id)
{
}

int TCharacteristicExpression::Value(TGameContext& ctx) const
{
    assert(ctx.gameObjectStorage != nullptr);
    TCreature* creature = ctx.gameObjectStorage->Get(creature_id_);
    assert(creature != nullptr);

    return creature->GetCharacteristic(type_).GetMod();
}

TWeaponBonusAttackExpression::TWeaponBonusAttackExpression(TGameObjectId creature_id)
    : creature_id_(creature_id)
{
}

int TWeaponBonusAttackExpression::Value(TGameContext& ctx) const
{
    assert(ctx.gameObjectStorage != nullptr);
    TCreature* creature = ctx.gameObjectStorage->Get(creature_id_);
    assert(creature != nullptr);

    return creature->GetCharacteristic(ECharacteristic::Strength).GetMod();
}
