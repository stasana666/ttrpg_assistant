#include <pf2e_engine/action_blocks/add_condition.h>

#include <pf2e_engine/condition.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/effect_manager.h>
#include <pf2e_engine/scheduler.h>
#include <pf2e_engine/transformation/transformator.h>

#include <stdexcept>

static const TGameObjectId kConditionId = TGameObjectIdManager::Instance().Register("condition");
static const TGameObjectId kAttackerId = TGameObjectIdManager::Instance().Register("attacker");
static const TGameObjectId kTargetId = TGameObjectIdManager::Instance().Register("target");
static const TGameObjectId kValueId = TGameObjectIdManager::Instance().Register("value");

void FAddCondition::operator ()(std::shared_ptr<TActionContext> ctx) const
{
    const std::string name = input_.GetString(kConditionId);
    if (name == "MultipleAttackPenalty") {
        return RecordAttackHandle(ctx);
    }
    EConditionKind condition = EConditionKindFromString(name);
    switch (condition) {
        case EConditionKind::Frightened:
            return FrightenedHandle(ctx);
        case EConditionKind::Prone:
            return ProneHandle(ctx);
        case EConditionKind::Dying: {
            auto* target = std::get<TPlayer*>(input_.Get(kTargetId, ctx));
            auto execution = ctx->transformator->ExecutionContext();
            return execution.Invoke(TGuarded<TCreature>(*target->GetCreature()),
                                    &TCreature::SetDyingValue, input_.GetNumber(kValueId));
        }
        case EConditionKind::Wounded: {
            auto* target = std::get<TPlayer*>(input_.Get(kTargetId, ctx));
            return ctx->transformator->ChangeCondition(target->GetCreature(), condition,
                                                      input_.GetNumber(kValueId));
        }
        case EConditionKind::Unconscious: {
            auto* target = std::get<TPlayer*>(input_.Get(kTargetId, ctx));
            auto execution = ctx->transformator->ExecutionContext();
            return execution.Invoke(TGuarded<TCreature>(*target->GetCreature()),
                                    &TCreature::BecomeUnconscious);
        }
    }
    throw std::runtime_error("invalid EConditionKind value: FAddCondition");
}

void FAddCondition::RecordAttackHandle(std::shared_ptr<TActionContext> ctx) const
{
    TPlayer& attacker = *std::get<TPlayer*>(input_.Get(kAttackerId, ctx));
    auto execution = ctx->transformator->ExecutionContext();
    execution.Invoke(TGuarded<TCreature>(*attacker.GetCreature()), &TCreature::RecordAttack, 1);
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
