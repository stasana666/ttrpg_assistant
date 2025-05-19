#include <armor_class.h>
#include <creature.h>
#include <inventory/armor.h>

TArmorClass::TArmorClass(TArmorSlot& armor, TCharacteristic& dexterity)
    : ac_bonus(armor.Get().AcBonus())
    , dex_cap(armor.Get().DexCap())
    , dex(dexterity.GetMod())
{
    armor.Subscribe([this](const TArmor& armor) -> void {
        this->ac_bonus = armor.AcBonus();
        this->dex_cap = armor.DexCap();
    });

    dexterity.Subscribe([this](const TCharacteristic& dexterity) -> void {
        this->dex = dexterity.GetMod();
    });
}

int TArmorClass::GetAc() const
{
    return 10 + ac_bonus + std::min(dex, dex_cap);
}
