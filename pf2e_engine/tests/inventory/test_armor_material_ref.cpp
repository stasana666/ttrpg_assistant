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

}  // namespace

// Loads steel.json then fullplate.json through the production factory, then
// reads fullplate back out and confirms its TMaterial ref resolved correctly.
// This is the headline proof that `ref T` + import in .ttrpg flows end-to-end.
TEST(ArmorMaterialRefTest, FullplateRefersToSteel) {
    TGameObjectFactory factory;
    factory.AddSource(kRootDirPath + "/pf2e_engine/data/inventory/material/steel.json");
    factory.AddSource(kRootDirPath + "/pf2e_engine/data/inventory/armor/fullplate.json");

    TArmor fullplate = factory.Create<TArmor>(NameId("fullplate"));

    EXPECT_EQ(fullplate.Category(), EArmorCategory::Heavy);
    EXPECT_EQ(fullplate.ArmorClassBonus(), 6);
    EXPECT_EQ(fullplate.DexterityCap(), 0);
    EXPECT_EQ(fullplate.Material().Hardness(), 10);
}

// Reversing source-add order proves the deferred-lambda pattern: refs resolve
// at Create-time, so it doesn't matter whether the referenced object was
// registered before or after the referring object.
TEST(ArmorMaterialRefTest, OrderIndependentLoading) {
    TGameObjectFactory factory;
    factory.AddSource(kRootDirPath + "/pf2e_engine/data/inventory/armor/fullplate.json");
    factory.AddSource(kRootDirPath + "/pf2e_engine/data/inventory/material/steel.json");

    TArmor fullplate = factory.Create<TArmor>(NameId("fullplate"));
    EXPECT_EQ(fullplate.Material().Hardness(), 10);
}
