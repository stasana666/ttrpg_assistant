#include <gtest/gtest.h>

#include <game_object_factory.h>
#include <cpp_config.h>

#include <filesystem>

const std::filesystem::path kPathToData{kRootDirPath + "/pf2e_engine/data"};
const std::filesystem::path kPathToArmor{kRootDirPath + "/pf2e_engine/data/inventory/armor"};

TEST(GameObjectFactory, ArmorFactory) {
    TGameObjectFactory factory;

    factory.AddSource(kPathToArmor);
}
