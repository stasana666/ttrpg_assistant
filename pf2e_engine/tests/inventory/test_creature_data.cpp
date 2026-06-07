#include <gtest/gtest.h>

#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/combat_calculator.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/expressions/number_expression.h>
#include <pf2e_engine/inventory/creature_data.h>
#include <pf2e_engine/inventory/creature_parts.h>
#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/mechanics/damage.h>

#include <cpp_config.h>

#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace {

TGameObjectId NameId(const std::string& name) {
    return TGameObjectIdManager::Instance().Register(name);
}

TGameObjectFactory MakeFactoryWithRefs() {
    TGameObjectFactory factory;
    factory.AddSource(kRootDirPath + "/pf2e_engine/data/inventory/material/steel.json");
    factory.AddSource(kRootDirPath + "/pf2e_engine/data/inventory/armor/fullplate.json");
    factory.AddSource(kRootDirPath + "/pf2e_engine/data/inventory/weapon/longsword.json");
    factory.AddSource(kRootDirPath + "/pf2e_engine/data/creatures/races/human.json");
    factory.AddSource(kRootDirPath + "/pf2e_engine/data/creatures/classes/fighter.json");
    factory.AddSource(kRootDirPath + "/pf2e_engine/data/creatures/ability_scores/warrior_abilities.json");
    return factory;
}

template <class T>
concept HasDamageResolverAccessor = requires(const T& creature) {
    creature.DamageResolver();
};

static_assert(!HasDamageResolverAccessor<TCreature>);

class TUnusedRng final : public IRandomGenerator {
public:
    int RollDice(int) final { return 1; }
};

}

TEST(CreatureDataTest, WarriorDataLoads) {
    TGameObjectFactory factory = MakeFactoryWithRefs();
    factory.AddSource(kRootDirPath + "/pf2e_engine/data/creatures/warrior_data.json");

    const TCreatureData data = factory.Create<TCreatureData>(NameId("warrior_data"));

    EXPECT_EQ(data.Level(), 1);
    EXPECT_EQ(data.Movement(), 5);

    EXPECT_EQ(data.Race().Hitpoints(), 8);
    EXPECT_EQ(data.Class().Hitpoints(), 10);
    EXPECT_EQ(data.Characteristic().Constitution().Value(), 16);
    EXPECT_EQ(data.Characteristic().Constitution().Modifier(), 3);
    EXPECT_EQ(data.Characteristic().Strength().Value(), 18);
    EXPECT_EQ(data.Characteristic().Strength().Modifier(), 4);

    EXPECT_EQ(data.Hitpoints().CurrentValue(), 21);
    EXPECT_EQ(data.Hitpoints().MaxValue(), 21);

    EXPECT_EQ(data.Armor().Category(), EArmorCategory::Heavy);
    EXPECT_EQ(data.Armor().ArmorClassBonus(), 6);

    ASSERT_EQ(data.Weapons().Size(), 1u);
    const TWeapon& weapon = *(*data.Weapons().begin());
    EXPECT_EQ(weapon.Category(), EWeaponCategory::Martial);
    EXPECT_EQ(weapon.BaseDiceSize(), 6);
    EXPECT_EQ(data.NaturalWeapons().Size(), 0u);
}

namespace {

nlohmann::json WarriorJson() {
    return {
        {"level", 3},
        {"race", "human"},
        {"class", "fighter"},
        {"characteristic", "warrior_abilities"},
        {"armor", "fullplate"},
        {"weapons", {"longsword"}},
    };
}

}

TEST(CreatureDataTest, ComputedHitpointsFallback) {
    TGameObjectFactory factory = MakeFactoryWithRefs();
    const TCreatureData data = TCreatureData::FromJson(WarriorJson(), factory);

    EXPECT_EQ(data.Level(), 3);
    EXPECT_EQ(data.Movement(), 0);
    EXPECT_EQ(data.Hitpoints().CurrentValue(), 47);
    EXPECT_EQ(data.Hitpoints().MaxValue(), 47);
}

TEST(CreatureDataTest, ComputedHitpointsJsonOverride) {
    TGameObjectFactory factory = MakeFactoryWithRefs();
    nlohmann::json j = WarriorJson();
    j["hitpoints"] = {{"current_value", 5}, {"max_value", 40}};

    const TCreatureData data = TCreatureData::FromJson(j, factory);

    EXPECT_EQ(data.Hitpoints().CurrentValue(), 5);
    EXPECT_EQ(data.Hitpoints().MaxValue(), 40);
}

TEST(CreatureDataTest, DamageDataLoadsAndIsAvailableOnCreature) {
    TGameObjectFactory factory = MakeFactoryWithRefs();
    nlohmann::json j = WarriorJson();
    j["immunities"] = {"Fire"};
    j["resistances"] = {
        {"Fire", 10},
        {"Slashing", 5},
    };
    j["vulnerabilities"] = {
        {"Piercing", 7},
    };

    TCreatureData data = TCreatureData::FromJson(j, factory);
    const TCreatureData& data_view = data;

    EXPECT_TRUE(data_view.Immunities().contains(EDamageType::Fire));
    EXPECT_EQ(data_view.Resistances().at(EDamageType::Fire), 10);
    EXPECT_EQ(data_view.Resistances().at(EDamageType::Slashing), 5);
    EXPECT_EQ(data_view.Vulnerabilities().at(EDamageType::Piercing), 7);

    TCreature creature(std::move(data), TProficiency(3), THitPoints(1));
    const TCreature& creature_view = creature;

    EXPECT_TRUE(creature_view.Immunities().contains(EDamageType::Fire));
    EXPECT_EQ(creature_view.Resistances().at(EDamageType::Fire), 10);
    EXPECT_EQ(creature_view.Vulnerabilities().at(EDamageType::Piercing), 7);

    TDamage damage;
    damage.Add(EDamageType::Slashing, std::make_unique<TNumberExpression>(9));
    damage.Add(EDamageType::Piercing, std::make_unique<TNumberExpression>(4));

    TUnusedRng rng;
    TCombatCalculator calculator;
    EXPECT_EQ(calculator.ResolveDamage(creature, damage, rng), 15);
}
