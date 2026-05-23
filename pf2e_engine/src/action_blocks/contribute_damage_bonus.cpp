#include <contribute_damage_bonus.h>

#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/expressions/dice_expression_parser.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/mechanics/damage.h>

#include <variant>

static const TGameObjectId kDiceId = TGameObjectIdManager::Instance().Register("dice");
static const TGameObjectId kDamageTypeId = TGameObjectIdManager::Instance().Register("damage_type");
static const TGameObjectId kDamageBonusId = TGameObjectIdManager::Instance().Register("damage_bonus");

FContributeDamageBonus::FContributeDamageBonus(TBlockInput&& input, TGameObjectId output)
    : FBaseFunction(std::move(input), output)
{
    dice_ = input_.GetString(kDiceId);
    damage_type_ = input_.GetString(kDamageTypeId);
}

void FContributeDamageBonus::operator()(std::shared_ptr<TActionContext> ctx) const
{
    // The hooked block (e.g. weapon_damage_roll) folds in $damage_bonus if it
    // exists. Lazy-create here so feats need no pre-setup from the host block.
    if (!ctx->game_object_registry->Contains(kDamageBonusId)) {
        ctx->game_object_registry->Add(kDamageBonusId, std::make_shared<TDamage>());
    }
    auto damage_bonus = std::get<std::shared_ptr<TDamage>>(input_.Get(kDamageBonusId, ctx));
    damage_bonus->Add(DamageTypeFromString(damage_type_), ParseDiceExpression(dice_));
}
