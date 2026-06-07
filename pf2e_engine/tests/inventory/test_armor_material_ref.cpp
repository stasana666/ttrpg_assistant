#include <gtest/gtest.h>

#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/inventory/material.h>

#include <cpp_config.h>

namespace {

TGameObjectId NameId(const std::string& name) {
    return TGameObjectIdManager::Instance().Register(name);
}

}

TEST(ArmorMaterialRefTest, FullplateRefersToSteel) {
    TGameObjectFactory factory;
    factory.AddSource(kRootDirPath + "/pf2e_engine/data/inventory/material/steel.json");
    factory.AddSource(kRootDirPath + "/pf2e_engine/data/inventory/armor/fullplate.json");

    const TArmor fullplate = factory.Create<TArmor>(NameId("fullplate"));

    EXPECT_EQ(fullplate.Category(), EArmorCategory::Heavy);
    EXPECT_EQ(fullplate.ArmorClassBonus(), 6);
    EXPECT_EQ(fullplate.DexterityCap(), 0);
    EXPECT_EQ(fullplate.Material().Hardness(), 10);
}

TEST(ArmorMaterialRefTest, OrderIndependentLoading) {
    TGameObjectFactory factory;
    factory.AddSource(kRootDirPath + "/pf2e_engine/data/inventory/armor/fullplate.json");
    factory.AddSource(kRootDirPath + "/pf2e_engine/data/inventory/material/steel.json");

    const TArmor fullplate = factory.Create<TArmor>(NameId("fullplate"));
    EXPECT_EQ(fullplate.Material().Hardness(), 10);
}
