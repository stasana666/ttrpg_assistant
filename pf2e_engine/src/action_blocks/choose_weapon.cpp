#include <choose_weapon.h>

#include <pf2e_engine/player.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>

static const TGameObjectId kCreatureId = TGameObjectIdManager::Instance().Register("creature");

void FChooseWeapon::operator ()(TActionContext& ctx) const
{
    TPlayer* player = std::get<TPlayer*>(input_.Get(kCreatureId, ctx));
    TWeaponSlots& weapons = player->GetCreature()->Weapons();
    if (weapons.Empty()) {
        throw std::logic_error("can't find weapon");
    }
    if (weapons.Size() > 1) {
        // TODO: нужно спрашивать пользователя каким оружием он хочет ударить, а в дальнейшем добавить более сложную логику.
        // Сейчас выбираем первое в списке - условно в ведущей руке
        ctx.game_object_registry->Add(output_, TGameObjectPtr{&weapons[0].Weapon()});
        return;
    }
    ctx.game_object_registry->Add(output_, TGameObjectPtr{&weapons[0].Weapon()});
}
