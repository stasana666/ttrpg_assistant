#include <pf2e_engine/creature.h>

#include <pf2e_engine/actions/action.h>
#include <pf2e_engine/common/errors.h>
#include <pf2e_engine/proficiency.h>

TCreature::TCreature(TCharacteristicSet stats, TProficiency proficiency, TArmor armor, THitPoints hitpoints)
    : stats_(stats)
    , proficiency_(proficiency)
    , hitpoints_(hitpoints)
    , armor_(armor)
{
}

const TCharacteristic& TCreature::GetCharacteristic(ECharacteristic name) const
{
    return stats_[name];
}

int TCreature::GetLevel() const
{
    return proficiency_.GetLevel();
}

TCharacteristicSet& TCreature::Characteristics()
{
    return stats_;
}

THitPoints& TCreature::Hitpoints()
{
    return hitpoints_;
}

const TResourcePool& TCreature::Resources() const
{
    return resources_;
}

TResourcePool& TCreature::Resources()
{
    return resources_;
}

const TArmor& TCreature::Armor() const
{
    return armor_;
}

TWeaponSlots& TCreature::Weapons()
{
    return weapons_;
}

const TProficiency& TCreature::Proficiency() const
{
    return proficiency_;
}

TProficiency& TCreature::Proficiency()
{
    return proficiency_;
}

const TDamageResolver& TCreature::DamageResolver() const
{
    return resolver_;
}

bool TCreature::IsAlive() const
{
    return hitpoints_.GetCurrentHp() > 0;
}

void TCreature::AddAction(std::shared_ptr<TAction> action)
{
    actions_.emplace_back(action);
}

std::vector<std::shared_ptr<TAction>>& TCreature::Actions()
{
    return actions_;
}

int TCreature::Get(ECondition condition) const
{
    auto it = conditions_.find(condition);
    if (it == conditions_.end()) {
        return 0;
    }
    return it->second;
}

void TCreature::Set(ECondition condition, int value)
{
    conditions_[condition] = value;
}

int& TCreature::Movement()
{
    return movement_;
}

std::vector<const TReaction*> TCreature::Reactions(ETrigger trigger_type) const
{
    std::vector<const TReaction*> reactions;
    for (auto& reaction : reactions_) {
        if (reaction->TriggerType() == trigger_type) {
            reactions.emplace_back(reaction.get());
        }
    }
    return reactions;
}
