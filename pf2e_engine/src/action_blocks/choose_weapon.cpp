#include <choose_weapon.h>

static const TGameObjectId kCreatureId = TGameObjectIdManager::Instance().Register("creature");

void FChooseWeapon::operator ()(TActionContext& ctx) const
{
    TCreature* creature = std::get<TCreature*>(input_.Get(kCreatureId, ctx));
    TWeaponSlots& weapons = creature->Weapons();
    if (weapons.Empty()) {
        throw std::logic_error("can't find weapon");
    }
    if (weapons.Size() > 1) {
        // TODO: нужно спрашивать пользователя каким оружием он хочет ударить, а в дальнейшем добавить более сложную логику.
        // Сейчас выбираем первое в списке - условно в ведущей руке
        ctx.game_object_register.Add(output_, TGameObjectPtr{&weapons[0].Weapon()});
        return;
    }
    ctx.game_object_register.Add(output_, TGameObjectPtr{&weapons[0].Weapon()});
}
