#include <choose_weapon.h>

#include <pf2e_engine/player.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>

static const TGameObjectId kCreatureId = TGameObjectIdManager::Instance().Register("creature");

void FChooseWeapon::operator ()(std::shared_ptr<TActionContext> ctx) const
{
    TPlayer* player = std::get<TPlayer*>(input_.Get(kCreatureId, ctx));
    TWeaponSlots& inventory = player->GetCreature()->Weapons();
    std::vector<TWeapon>& natural = player->GetCreature()->NaturalWeapons();
    // TODO: ask the user which weapon to use; for now pick the first available,
    // preferring an inventory weapon over a natural one.
    if (!inventory.Empty()) {
        ctx->game_object_registry->Add(output_, TGameObjectPtr{&inventory[0].Weapon()});
        return;
    }
    if (!natural.empty()) {
        ctx->game_object_registry->Add(output_, TGameObjectPtr{&natural.front()});
        return;
    }
    throw std::logic_error("can't find weapon");
}
