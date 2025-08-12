#include <gtest/gtest.h>

#include <game_object_factory.h>
#include <cpp_config.h>

#include <filesystem>

const std::filesystem::path kPathToData{kRootDirPath + "/pf2e_engine/data"};
const std::filesystem::path kPathToArmor{kRootDirPath + "/pf2e_engine/data/inventory/armor"};
const std::filesystem::path kPathToWeapon{kRootDirPath + "/pf2e_engine/data/inventory/weapon"};
const std::filesystem::path kPathToCreature{kRootDirPath + "/pf2e_engine/data/creatures"};

TEST(GameObjectFactory, ArmorFactory) {
    TGameObjectFactory factory;

    factory.AddSource(kPathToArmor);
}

TEST(GameObjectFactory, WeaponFactory) {
    TGameObjectFactory factory;

    factory.AddSource(kPathToWeapon);
}

// TEST(GameObjectFactory, CreatureFactory) {
//     TGameObjectFactory factory;

//     factory.AddSource(kPathToCreature);
// }
