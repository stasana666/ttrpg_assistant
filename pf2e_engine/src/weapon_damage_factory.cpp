#include <weapon_damage_factory.h>

#include <pf2e_engine/expressions/dice_expression.h>
#include <pf2e_engine/expressions/number_expression.h>
#include <pf2e_engine/expressions/math_expression.h>

TWeaponDamageFactory::TWeaponDamageFactory(TWeaponSlot& weapon, TCharacteristic& strength)
    : str_(strength.GetMod())
    , weapon_(weapon.Get())
{
    strength.Subscribe([this](const TCharacteristic& strength) {
        this->str_ = strength.GetMod();
    });

    weapon.Subscribe([this](const TWeapon* weapon) -> void {
        this->weapon_ = weapon;
    });
}

TDamage TWeaponDamageFactory::HitDamage() const
{
    if (weapon_ == nullptr) {
        throw std::logic_error("not supported yet");
    }
    TDamage result;
    result.Add(
        std::make_unique<TSumExpression>(
            std::make_unique<TDiceExpression>(weapon_->GetBaseDieSize()),
            std::make_unique<TNumberExpression>(str_)
        ),
        weapon_->GetDamageType()
    );
    return result;
}

TDamage TWeaponDamageFactory::CritDamage() const
{
    if (weapon_ == nullptr) {
        throw std::logic_error("not supported yet");
    }
    TDamage result;
    result.Add(
        std::make_unique<TProductExpression>(
            std::make_unique<TSumExpression>(
                std::make_unique<TDiceExpression>(weapon_->GetBaseDieSize()),
                std::make_unique<TNumberExpression>(str_)
            ),
            std::make_unique<TNumberExpression>(2)
        ),
        weapon_->GetDamageType()
    );
    return result;
}
