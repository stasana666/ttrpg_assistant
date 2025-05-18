#include <armour_class.h>
#include <creature.h>
#include <inventory/armour.h>

TArmourClass::TArmourClass()
    : creature(nullptr)
{
}

void TArmourClass::Bind(const TCreature *creature_)
{
    creature = creature_;
}

int TArmourClass::GetAc() const
{
    const TArmour& armour = creature->GetArmour();
    return armour.AcBonus() + std::min(creature->GetCharacteristic(ECharacteristic::Dexterity).GetMod(), armour.DexCap());
}
