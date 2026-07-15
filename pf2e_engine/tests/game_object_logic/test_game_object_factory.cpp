#include <gtest/gtest.h>

#include <game_object_factory.h>
#include <game_object_registry.h>
#include <game_object_id.h>
#include <cpp_config.h>

#include <filesystem>
#include <variant>

TEST(GameObjectRegistry, AddRebindsExistingId) {
    TGameObjectRegistry registry;
    TGameObjectId id = TGameObjectIdManager::Instance().Register("rebind_slot");

    registry.Add(id, 1);
    registry.Add(id, 2);

    // TForEachBlock rebinds its element id every iteration; Add must overwrite,
    // not silently keep the first binding.
    EXPECT_EQ(std::get<int>(registry.GetGameObjectPtr(id)), 2);
}

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

TEST(GameObjectFactory, CreatureFactory) {
    TGameObjectFactory factory;

    factory.AddSource(kPathToCreature);
}
