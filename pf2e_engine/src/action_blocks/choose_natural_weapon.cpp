#include <choose_natural_weapon.h>

#include <pf2e_engine/creature.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/player.h>

#include <stdexcept>

static const TGameObjectId kCreatureId = TGameObjectIdManager::Instance().Register("creature");
static const TGameObjectId kNameId = TGameObjectIdManager::Instance().Register("name");

FChooseNaturalWeapon::FChooseNaturalWeapon(TBlockInput&& input, TGameObjectId output)
    : FBaseFunction(std::move(input), output)
{
    name_ = input_.GetString(kNameId);
}

void FChooseNaturalWeapon::operator()(std::shared_ptr<TActionContext> ctx) const
{
    TPlayer* player = std::get<TPlayer*>(input_.Get(kCreatureId, ctx));
    auto& natural_weapons = player->GetCreature()->NaturalWeapons();
    for (TWeapon& weapon : natural_weapons) {
        if (weapon.Name() == name_) {
            ctx->game_object_registry->Add(output_, TGameObjectPtr{&weapon});
            return;
        }
    }
    throw std::runtime_error("natural weapon not found: " + name_);
}
