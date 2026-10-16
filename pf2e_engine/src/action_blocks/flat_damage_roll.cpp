#include <pf2e_engine/action_blocks/flat_damage_roll.h>

#include <pf2e_engine/expressions/dice_expression_parser.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/mechanics/damage.h>

#include <stdexcept>

static const TGameObjectId kDiceId = TGameObjectIdManager::Instance().Register("dice");
static const TGameObjectId kDamageTypeId = TGameObjectIdManager::Instance().Register("damage_type");
static const TGameObjectId kCriticalId = TGameObjectIdManager::Instance().Register("critical");
static const TGameObjectId kNonlethalId = TGameObjectIdManager::Instance().Register("nonlethal");

void FFlatDamageRoll::operator()(std::shared_ptr<TActionContext> ctx) const
{
    std::string dice = input_.GetString(kDiceId);
    EDamageType damage_type = EDamageTypeFromString(input_.GetString(kDamageTypeId));
    const int critical = input_.Has(kCriticalId) ? input_.GetNumber(kCriticalId) : 0;
    const int nonlethal = input_.Has(kNonlethalId) ? input_.GetNumber(kNonlethalId) : 0;
    if ((critical != 0 && critical != 1) || (nonlethal != 0 && nonlethal != 1)) {
        throw std::invalid_argument("damage context flags must be zero or one");
    }

    auto damage = std::make_shared<TDamage>(TDamageContext::Create({
        .Critical = critical != 0, .Nonlethal = nonlethal != 0,
    }));
    damage->Add(damage_type, ParseDiceExpression(dice));

    ctx->game_object_registry->Add(output_, damage);
}
