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

namespace {

const TWeapon* GetWeapon(const TGameObjectPtr& object)
{
    if (const auto* weapon = std::get_if<TWeapon*>(&object)) {
        return *weapon;
    }
    if (const auto* weapon = std::get_if<const TWeapon*>(&object)) {
        return *weapon;
    }
    throw std::logic_error("expected weapon");
}

}

void FAddCondition::operator ()(std::shared_ptr<TActionContext> ctx) const
{
    EConditionKind condition = EConditionKindFromString(input_.GetString(kConditionId));
    switch (condition) {
        case EConditionKind::MultipleAttackPenalty:
            return MultipleAttackPenaltyHandle(ctx);
        case EConditionKind::Frightened:
            return FrightenedHandle(ctx);
        case EConditionKind::Prone:
            return ProneHandle(ctx);
    }
    throw std::runtime_error("invalid EConditionKind value: FAddCondition");
}

void FAddCondition::MultipleAttackPenaltyHandle(std::shared_ptr<TActionContext> ctx) const
{
    TPlayer& attacker = *std::get<TPlayer*>(input_.Get(kAttackerId, ctx));

    int current = attacker.GetCreature()->Get(EConditionKind::MultipleAttackPenalty);

    int increase = 5;
    if (input_.Has(kWeaponId)) {
        const TWeapon* weapon = GetWeapon(input_.Get(kWeaponId, ctx));
        if (weapon->Traits().Has(EWeaponTraitKind::Agile)) {
            increase = 4;
        }
    }

    auto canceler = ctx->effect_manager->AddEffect(TPlayerConditionSet{
        .player = &attacker,
        .condition = EConditionKind::MultipleAttackPenalty,
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
        .condition = EConditionKind::Frightened,
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
    TPlayer& target = *std::get<TPlayer*>(input_.Get(kTargetId, ctx));
    ctx->transformator->ChangeCondition(target.GetCreature(), EConditionKind::Prone, 1);
}
