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
