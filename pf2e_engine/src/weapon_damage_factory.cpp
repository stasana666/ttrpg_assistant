#include <weapon_damage_factory.h>

TWeaponDamageFactory::TWeaponDamageFactory(TWeaponSlot& weapon, TCharacteristic& strength)
    : str(strength.GetMod())
    , weapon(weapon.Get())
{
    strength.Subscribe([this](const TCharacteristic& strength) {
        this->str = strength.GetMod();
    });

    weapon.Subscribe([this](const TWeapon* weapon) -> void {
        this->weapon = weapon;
    });
}

TDamage TWeaponDamageFactory::HitDamage() const
{
    if (weapon == nullptr) {
        throw std::logic_error("not supported yet");
    }
    TDamage result;
    result.Add(
        std::make_unique<TSumExpression>(
            std::make_unique<TDice>(weapon->GetBaseDieSize()),
            std::make_unique<TNumber>(str)
        ),
        weapon->GetDamageType()
    );
    return result;
}

TDamage TWeaponDamageFactory::CritDamage() const
{
    if (weapon == nullptr) {
        throw std::logic_error("not supported yet");
    }
    TDamage result;
    result.Add(
        std::make_unique<TMultiplyExpression>(
            std::make_unique<TSumExpression>(
                std::make_unique<TDice>(weapon->GetBaseDieSize()),
                std::make_unique<TNumber>(str)
            ),
            std::make_unique<TNumber>(2)
        ),
        weapon->GetDamageType()
    );
    return result;
}
