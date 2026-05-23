#pragma once

#include <pf2e_engine/action_blocks/base_function.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>

#include <string>

// Picks a natural weapon (jaws, claws, ...) off the given creature by name.
// The weapon definition lives on the creature (TCreature::NaturalWeapons),
// not in inventory and not in the action's variables, so the same attack
// action can be reused by any creature that owns a natural weapon of that
// name.
class FChooseNaturalWeapon : public FBaseFunction {
public:
    FChooseNaturalWeapon(TBlockInput&& input, TGameObjectId output);

    void operator()(std::shared_ptr<TActionContext> ctx) const;

private:
    std::string name_;
};
