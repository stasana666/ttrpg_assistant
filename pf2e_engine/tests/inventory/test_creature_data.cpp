#include <gtest/gtest.h>

#include <pf2e_engine/creature.h>

#include <map>
#include <set>
#include <utility>

namespace {

TCreature::TInitialValues WarriorValues()
{
    const auto score = [](int value) { return TAbilityScore::Create({.Value = value}); };
    return {
        .Level = 3,
        .Size = ECreatureSize::Medium,
        .Race = TRace::Create({.Hitpoints = 8}),
        .Class = TClass::Create({.Hitpoints = 10}),
        .Characteristic = TAbilityScores::Create({
            .Strength = score(18), .Dexterity = score(12), .Constitution = score(16),
            .Intelligence = score(10), .Wisdom = score(12), .Charisma = score(10),
        }),
    };
}

}

TEST(CreatureDataTest, ComputedHitpointsFallback) {
    const TCreature data = TCreature::Create(WarriorValues());

    EXPECT_EQ(data.Level(), 3);
    EXPECT_EQ(data.Movement(), 0);
    EXPECT_EQ(data.Hitpoints().Current(), 47);
    EXPECT_EQ(data.Hitpoints().Maximum().Value(), 47);
}

TEST(CreatureDataTest, ExplicitHitpointsOverrideTheComputedDefault) {
    auto values = WarriorValues();
    values.Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 40}), .Current = 5});

    const TCreature data = TCreature::Create(std::move(values));

    EXPECT_EQ(data.Hitpoints().Current(), 5);
    EXPECT_EQ(data.Hitpoints().Maximum().Value(), 40);
}

TEST(CreatureDataTest, DamageDataIsAvailableOnCreature) {
    auto values = WarriorValues();
    values.Immunities = std::set<EDamageType>{EDamageType::Fire};
    values.Resistances = std::map<EDamageType, int>{{EDamageType::Fire, 10}, {EDamageType::Slashing, 5}};
    values.Vulnerabilities = std::map<EDamageType, int>{{EDamageType::Piercing, 7}};

    TCreature data = TCreature::Create(std::move(values));
    TCreature creature(std::move(data));
    const TCreature& creature_view = creature;

    EXPECT_TRUE(creature_view.Immunities().contains(EDamageType::Fire));
    EXPECT_EQ(creature_view.Resistances().at(EDamageType::Fire), 10);
    EXPECT_EQ(creature_view.Resistances().at(EDamageType::Slashing), 5);
    EXPECT_EQ(creature_view.Vulnerabilities().at(EDamageType::Piercing), 7);

    EXPECT_EQ(creature_view.DamageAmount(EDamageType::Slashing, 9), 4);
    EXPECT_EQ(creature_view.DamageAmount(EDamageType::Piercing, 4), 11);
}
