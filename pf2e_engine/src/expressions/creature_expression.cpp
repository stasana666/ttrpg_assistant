#include "creature_expression.h"

#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/game_object_logic/game_object_storage.h>
#include <pf2e_engine/game_context.h>

#include <cassert>
#include <variant>


TArmorClassExpression::TArmorClassExpression(TGameObjectId creature_id)
    : creature_id_(creature_id)
{
}

int TArmorClassExpression::Value(TGameContext& ctx) const
{
    assert(ctx.gameObjectStorage != nullptr);
    TGameObject& object = ctx.gameObjectStorage->GetRef(creature_id_);
    assert(std::holds_alternative<TCreature>(object));
    TCreature& creature = std::get<TCreature>(object);

    const TArmor& armor = creature.Armor();
    return 10 + armor.AcBonus() + std::min(armor.DexCap(), creature.GetCharacteristic(ECharacteristic::Dexterity).GetMod());
}

TCharacteristicExpression::TCharacteristicExpression(ECharacteristic type, TGameObjectId creature_id)
    : type_(type)
    , creature_id_(creature_id)
{
}

int TCharacteristicExpression::Value(TGameContext& ctx) const
{
    assert(ctx.gameObjectStorage != nullptr);
    TGameObject& object = ctx.gameObjectStorage->GetRef(creature_id_);
    assert(std::holds_alternative<TCreature>(object));
    TCreature& creature = std::get<TCreature>(object);

    return creature.GetCharacteristic(type_).GetMod();
}

TWeaponBonusAttackExpression::TWeaponBonusAttackExpression(TGameObjectId creature_id)
    : creature_id_(creature_id)
{
}

int TWeaponBonusAttackExpression::Value(TGameContext& ctx) const
{
    assert(ctx.gameObjectStorage != nullptr);
    TGameObject& object = ctx.gameObjectStorage->GetRef(creature_id_);
    assert(std::holds_alternative<TCreature>(object));
    TCreature& creature = std::get<TCreature>(object);

    return creature.GetCharacteristic(ECharacteristic::Strength).GetMod();
}
