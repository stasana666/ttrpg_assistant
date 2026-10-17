#include <pf2e_engine/rules.h>

#include <pf2e_engine/creature.h>
#include <pf2e_engine/inventory/abilities.h>
#include <pf2e_engine/inventory/combat_timing.h>
#include <pf2e_engine/inventory/flanking.h>
#include <pf2e_engine/inventory/intimidation.h>
#include <pf2e_engine/inventory/movement.h>
#include <pf2e_engine/inventory/remaster_profiles.h>
#include <pf2e_engine/inventory/shield_actions.h>
#include <pf2e_engine/inventory/trip.h>

std::shared_ptr<const TAbilityRules> CreateAbilityRules()
{
    auto rules = std::make_shared<TAbilityRules>();
    rules->Time.RoundDurationSeconds = CreateCombatTiming().RoundSeconds();
    RegisterAbilitiesRules(*rules);
    RegisterFlankingRules(*rules);
    RegisterMovementRules(*rules);
    RegisterShieldActionsRules(*rules);
    RegisterIntimidationRules(*rules);
    RegisterTripRules(*rules);
    RegisterRemasterProfilesRules(*rules);
    rules->Lifecycle.Register(ELifecyclePhase::TurnStartExpiration,
        std::make_shared<const TLifecycleExecution>(TLifecycleExecution{
            .OwnerType = typeid(TCreature),
            .Run = RunAbilitiesExpireCreatureTurnStart,
        }));
    rules->Lifecycle.Register(ELifecyclePhase::TurnStart,
        std::make_shared<const TLifecycleExecution>(TLifecycleExecution{
            .OwnerType = typeid(TCreature),
            .Run = RunAbilitiesStartCreatureTurn,
        }));
    return rules;
}
