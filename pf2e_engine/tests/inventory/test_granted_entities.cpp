#include <pf2e_engine/inventory/granted_entities.h>
#include <pf2e_engine/common/ability.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace {

const std::string kStrike = "granted_abilities.FixtureStrike";
const std::string kStride = "granted_abilities.FixtureStride";
const std::string kSuddenCharge = "granted_abilities.FixtureSuddenCharge";
const std::string kOpportunity = "granted_abilities.FixtureOpportunity";
const std::string kFighterTraining = "granted_abilities.FixtureFighterTraining";
const std::string kNaturalAmbition = "granted_abilities.FixtureNaturalAmbition";
const std::string kVersatileTraining = "granted_abilities.FixtureVersatileTraining";

TAbilityCatalog MakeCatalog()
{
    TAbilityCatalog catalog;
    RegisterGrantedEntitiesAbilities(catalog);
    return catalog;
}

std::vector<TGrantedAbility> Collect(const TGrantCreature& creature,
                                   const TAbilityCatalog& catalog)
{
    TAbilityCollector collector(catalog);
    creature.VisitGrantedAbilities(collector);
    return collector.Grants();
}

std::vector<TGrantedAbility> Matching(const std::vector<TGrantedAbility>& grants,
                                    std::string_view definition)
{
    std::vector<TGrantedAbility> result;
    for (const auto& grant : grants) {
        if (grant.DefinitionId == definition) {
            result.push_back(grant);
        }
    }
    return result;
}

}

TEST(GrantedEntityTest, RegistrarIncludesImportedDefinitionsAndFeatureDependencies)
{
    const TAbilityCatalog catalog = MakeCatalog();
    ASSERT_NE(catalog.Find(kStrike), nullptr);
    ASSERT_NE(catalog.Find(kOpportunity), nullptr);
    ASSERT_NE(catalog.Find(kVersatileTraining), nullptr);
    EXPECT_EQ(catalog.Find(kStrike)->Kind, EAbilityKind::Action);
    EXPECT_EQ(catalog.Find(kOpportunity)->Kind, EAbilityKind::Reaction);
    EXPECT_EQ(catalog.Find(kVersatileTraining)->Grants,
              std::vector<std::string>{kFighterTraining});
}

TEST(GrantedEntityTest, NamedFighterAndTwoWolvesHaveIndependentIdentityAndCorrectKinds)
{
    const TAbilityCatalog catalog = MakeCatalog();
    const TGrantCreature fighter = CreateGrantFighter();
    const TGrantCreature first_wolf = CreateGrantWolf();
    const TGrantCreature second_wolf = CreateGrantWolf();

    EXPECT_EQ(fighter.Name(), "Human fighter");
    EXPECT_EQ(first_wolf.Name(), "Wolf");
    EXPECT_NE(fighter.Id(), first_wolf.Id());
    EXPECT_NE(first_wolf.Id(), second_wolf.Id());

    const auto fighter_grants = Collect(fighter, catalog);
    const auto first_grants = Collect(first_wolf, catalog);
    const auto second_grants = Collect(second_wolf, catalog);
    EXPECT_EQ(fighter_grants.size(), 4u);
    EXPECT_EQ(first_grants.size(), 2u);
    EXPECT_EQ(second_grants.size(), 2u);
    EXPECT_EQ(Matching(fighter_grants, kStrike).size(), 1u);
    EXPECT_EQ(Matching(fighter_grants, kSuddenCharge).size(), 1u);
    const auto reactions = Matching(fighter_grants, kOpportunity);
    ASSERT_EQ(reactions.size(), 1u);
    EXPECT_EQ(reactions.front().Kind, EAbilityKind::Reaction);
    EXPECT_TRUE(Matching(first_grants, kOpportunity).empty());
    EXPECT_TRUE(Matching(second_grants, kOpportunity).empty());
    EXPECT_EQ(std::count_if(first_grants.begin(), first_grants.end(),
        [](const TGrantedAbility& grant) { return grant.Kind == EAbilityKind::Action; }), 2);
}

TEST(GrantedEntityTest, LoadoutAndFeatureChoicesDriveTheAvailableAbilities)
{
    const TAbilityCatalog catalog = MakeCatalog();
    const auto creature = [](bool with_features) {
        TIdCollection<TFeatureSelection> dagger_features;
        TIdCollection<TFeatureSelection> features;
        if (with_features) {
            dagger_features.Add(TFeatureSelection(kNaturalAmbition));
            features.Add(TFeatureSelection(kVersatileTraining));
        }
        TIdCollection<TGrantArms> weapons;
        weapons.Add(TGrantArms::Create({.Name = "Dagger", .Features = std::move(dagger_features)}));
        weapons.Add(TGrantArms::Create({.Name = "Spear"}));
        return TGrantCreature::Create({
            .Name = "Human fighter", .Weapons = std::move(weapons), .Features = std::move(features),
        });
    };
    const TGrantCreature equipped = creature(true);
    const auto equipped_grants = Collect(equipped, catalog);
    EXPECT_EQ(equipped_grants.size(), 5u);
    EXPECT_EQ(Matching(equipped_grants, kStrike).size(), 2u);
    EXPECT_EQ(Matching(equipped_grants, kOpportunity).size(), 1u);
    const auto charge = Matching(equipped_grants, kSuddenCharge);
    ASSERT_EQ(charge.size(), 1u);
    EXPECT_FALSE(charge.front().Provenance.empty());

    const TGrantCreature without_features = creature(false);
    const auto ordinary_grants = Collect(without_features, catalog);
    EXPECT_EQ(ordinary_grants.size(), 3u);
    EXPECT_TRUE(Matching(ordinary_grants, kOpportunity).empty());
    EXPECT_TRUE(Matching(ordinary_grants, kSuddenCharge).empty());
}

TEST(GrantedEntityTest, IdenticalWeaponsKeepCollectionIdentityInsteadOfTraversalIndex)
{
    TIdCollection<TGrantArms> weapons;
    const auto removed = weapons.Add(TGrantArms::Create({.Name = "Dagger"}));
    const auto first = weapons.Add(TGrantArms::Create({.Name = "Dagger"}));
    weapons.Remove(removed);
    const auto second = weapons.Add(TGrantArms::Create({.Name = "Dagger"}));
    const TGrantCreature fighter = TGrantCreature::Create({
        .Name = "Human fighter", .Weapons = weapons
    });
    const TAbilityCatalog catalog = MakeCatalog();
    const auto strikes = Matching(Collect(fighter, catalog), kStrike);

    ASSERT_EQ(strikes.size(), 2u);
    EXPECT_NE(strikes[0].Source, strikes[1].Source);
    EXPECT_EQ(strikes[0].Source, TAbilitySourcePath{}.Field("weapons").Entry(first.Value));
    EXPECT_EQ(strikes[1].Source, TAbilitySourcePath{}.Field("weapons").Entry(second.Value));
    EXPECT_FALSE(strikes[0].Provenance.empty());
    EXPECT_FALSE(strikes[1].Provenance.empty());
}

TEST(GrantedEntityTest, RemovingOneWeaponSourceLeavesTheOtherGrantUnchanged)
{
    TIdCollection<TGrantArms> weapons;
    const auto dropped = weapons.Add(TGrantArms::Create({.Name = "Dagger"}));
    weapons.Add(TGrantArms::Create({.Name = "Dagger"}));
    const TGrantCreature fighter = TGrantCreature::Create({
        .Name = "Human fighter", .Weapons = weapons
    });
    const TAbilityCatalog catalog = MakeCatalog();
    const auto before = Matching(Collect(fighter, catalog), kStrike);
    ASSERT_EQ(before.size(), 2u);
    weapons.Remove(dropped);
    const TGrantCreature unequipped = TGrantCreature::Create({
        .Name = "Human fighter", .Weapons = weapons
    });
    const auto after = Matching(Collect(unequipped, catalog), kStrike);

    ASSERT_EQ(after.size(), 1u);
    EXPECT_EQ(after.front().Source, before.back().Source);
    EXPECT_EQ(after.front().Provenance, before.back().Provenance);
    EXPECT_EQ(Matching(Collect(unequipped, catalog), kStride).size(), 1u);
}
