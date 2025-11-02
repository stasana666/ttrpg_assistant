#include <pf2e_engine/action_blocks/add_condition.h>

#include <pf2e_engine/condition.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/effect_manager.h>
#include <pf2e_engine/scheduler.h>
#include <pf2e_engine/inventory/weapon.h>

#include <algorithm>
#include <stdexcept>

static const TGameObjectId kConditionId = TGameObjectIdManager::Instance().Register("condition");
static const TGameObjectId kAttackerId = TGameObjectIdManager::Instance().Register("attacker");
static const TGameObjectId kWeaponId = TGameObjectIdManager::Instance().Register("weapon");

void FAddCondition::operator ()(TActionContext& ctx) const
{
    ECondition condition = ConditionFromString(input_.GetString(kConditionId));
    switch (condition) {
        case ECondition::MultipleAttackPenalty:
            return MultipleAttackPenaltyHandle(ctx);
        case ECondition::COUNT:
            throw std::runtime_error("COUNT is not valid value of ECondition: FAddCondition");
    }
}

void FAddCondition::MultipleAttackPenaltyHandle(TActionContext& ctx) const
{
    TPlayer& attacker = *std::get<TPlayer*>(input_.Get(kAttackerId, ctx));

    int current = attacker.creature->Get(ECondition::MultipleAttackPenalty);

    TWeapon& weapon = *std::get<TWeapon*>(input_.Get(kWeaponId, ctx));

    int increase = weapon.HasTrait(EWeaponTrait::Agile) ? 4 : 5;

    auto canceler = ctx.effect_manager->AddEffect(TEffect{
        TPlayerConditionSet{
            .player = &attacker,
            .condition = ECondition::MultipleAttackPenalty,
            .value = std::min(10, current + increase),
        }
    });

    ctx.scheduler->AddTask(TTask{
        .events_before_call = {
            TEvent{
                .type = EEvent::OnTurnEnd,
                .context = {.player = &attacker },
            }
        },
        .callback = canceler,
    });
}
