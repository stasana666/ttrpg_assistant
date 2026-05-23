#include <pf2e_engine/action_blocks/add_condition.h>

#include <pf2e_engine/condition.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/effect_manager.h>
#include <pf2e_engine/scheduler.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/transformation/transformator.h>

#include <algorithm>
#include <stdexcept>

static const TGameObjectId kConditionId = TGameObjectIdManager::Instance().Register("condition");
static const TGameObjectId kAttackerId = TGameObjectIdManager::Instance().Register("attacker");
static const TGameObjectId kWeaponId = TGameObjectIdManager::Instance().Register("weapon");
static const TGameObjectId kTargetId = TGameObjectIdManager::Instance().Register("target");
static const TGameObjectId kValueId = TGameObjectIdManager::Instance().Register("value");

void FAddCondition::operator ()(std::shared_ptr<TActionContext> ctx) const
{
    ECondition condition = ConditionFromString(input_.GetString(kConditionId));
    switch (condition) {
        case ECondition::MultipleAttackPenalty:
            return MultipleAttackPenaltyHandle(ctx);
        case ECondition::Frightened:
            return FrightenedHandle(ctx);
        case ECondition::Prone:
            return ProneHandle(ctx);
        case ECondition::COUNT:
            throw std::runtime_error("COUNT is not valid value of ECondition: FAddCondition");
    }
}

void FAddCondition::MultipleAttackPenaltyHandle(std::shared_ptr<TActionContext> ctx) const
{
    TPlayer& attacker = *std::get<TPlayer*>(input_.Get(kAttackerId, ctx));

    int current = attacker.GetCreature()->Get(ECondition::MultipleAttackPenalty);

    // Weapon is optional: Strike provides one (Agile -> 4, non-Agile -> 5);
    // weaponless attack-trait actions (e.g. Trip) default to 5.
    int increase = 5;
    if (input_.Has(kWeaponId)) {
        TWeapon& weapon = *std::get<TWeapon*>(input_.Get(kWeaponId, ctx));
        if (weapon.HasTrait(EWeaponTrait::Agile)) {
            increase = 4;
        }
    }

    auto canceler = ctx->effect_manager->AddEffect(TPlayerConditionSet{
        .player = &attacker,
        .condition = ECondition::MultipleAttackPenalty,
        .value = std::min(10, current + increase),
    }, *ctx->transformator);

    ctx->transformator->AddTask(ctx->scheduler, TTask{
        .events_before_call = {
            TEvent{
                .type = EEvent::OnTurnEnd,
                .context = {.player = &attacker },
            }
        },
        .callback = [canceler]() { return canceler(EEffectCancelPolicy::Cancel); },
    });
}

void FAddCondition::FrightenedHandle(std::shared_ptr<TActionContext> ctx) const
{
    TPlayer& target = *std::get<TPlayer*>(input_.Get(kTargetId, ctx));
    int value = input_.GetNumber(kValueId);

    TPlayerConditionSet condition_set{
        .player = &target,
        .condition = ECondition::Frightened,
        .value = value,
    };

    auto canceler = ctx->effect_manager->AddEffect(condition_set, *ctx->transformator);

    TEvent event{
        .type = EEvent::OnTurnStart,
        .context = {.player = &target },
    };

    ctx->transformator->AddTask(ctx->scheduler, TTask{
        .events_before_call = { event },
        .callback = [canceler]() { return canceler(EEffectCancelPolicy::ReduceUntilZero); },
    });
}

void FAddCondition::ProneHandle(std::shared_ptr<TActionContext> ctx) const
{
    // Prone is a binary flag with no auto-expiration; Stand removes it.
    TPlayer& target = *std::get<TPlayer*>(input_.Get(kTargetId, ctx));
    ctx->transformator->ChangeCondition(target.GetCreature(), ECondition::Prone, 1);
}
