#include <pf2e_engine/action_blocks/flat_damage_roll.h>

#include <pf2e_engine/expressions/dice_expression_parser.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/mechanics/damage.h>

static const TGameObjectId kDiceId = TGameObjectIdManager::Instance().Register("dice");
static const TGameObjectId kDamageTypeId = TGameObjectIdManager::Instance().Register("damage_type");

void FFlatDamageRoll::operator()(std::shared_ptr<TActionContext> ctx) const
{
    std::string dice = input_.GetString(kDiceId);
    TDamage::Type damage_type = DamageTypeFromString(input_.GetString(kDamageTypeId));

    auto damage = std::make_shared<TDamage>();
    damage->Add(damage_type, ParseDiceExpression(dice));

    ctx->game_object_registry->Add(output_, damage);
}
