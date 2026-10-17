#include <pf2e_engine/inventory/remaster_profiles.h>
#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/rules.h>
#include <pf2e_engine/common/entity_id.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>
#include "../test_lib/state_diff.h"

namespace {

TEST(RemasterProfilesTest, FighterDefaultsMatchTheDeclaredLevelOneBuild)
{
    const auto fighter = CreateHumanFighter1();
    EXPECT_EQ(fighter.Name(), "Human fighter 1");
    EXPECT_EQ(fighter.Level(), 1);
    EXPECT_EQ(fighter.Size(), ECreatureSize::Medium);
    EXPECT_EQ(fighter.Characteristic().Strength().Modifier(), 4);
    EXPECT_EQ(fighter.Characteristic().Dexterity().Modifier(), 2);
    EXPECT_EQ(fighter.Characteristic().Constitution().Modifier(), 2);
    EXPECT_EQ(fighter.Characteristic().Intelligence().Modifier(), 0);
    EXPECT_EQ(fighter.Characteristic().Wisdom().Modifier(), 1);
    EXPECT_EQ(fighter.Characteristic().Charisma().Modifier(), 0);
    EXPECT_EQ(fighter.Hitpoints().Maximum().Value(), 21);
    EXPECT_EQ(fighter.Hitpoints().Current(), 21);
    EXPECT_EQ(fighter.Hitpoints().Temporary(), 0);
    EXPECT_EQ(fighter.ArmorClass(), 15);
    EXPECT_EQ(fighter.PerceptionBonus(), 6);
    EXPECT_EQ(fighter.SaveBonus(ESavethrow::Fortitude), 7);
    EXPECT_EQ(fighter.SaveBonus(ESavethrow::Reflex), 7);
    EXPECT_EQ(fighter.SaveBonus(ESavethrow::Will), 4);
    EXPECT_EQ(fighter.ClassDC(), 17);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Athletics), 7);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Acrobatics), 5);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Medicine), 4);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Survival), 4);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Intimidation), 3);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Stealth), 2);
    ASSERT_EQ(fighter.LoreSkills().Size(), 1U);
    EXPECT_EQ(fighter.LoreBase(*(*fighter.LoreSkills().begin())), 3);
    EXPECT_EQ(fighter.SpeedFeet(), 25);
    EXPECT_EQ(fighter.Hands().Count(), 0);
    EXPECT_EQ(fighter.ZeroHpRule(), EZeroHpRule::Dying);
    EXPECT_EQ(fighter.Languages().Size(), 2U);
    EXPECT_EQ(fighter.WeaponProficiency(EWeaponCategory::Advanced), 3);
    EXPECT_EQ(fighter.WeaponProficiency(EWeaponCategory::Martial), 5);
    EXPECT_TRUE(fighter.Shield().CanRaise());
    EXPECT_FALSE(fighter.Shield().IsRaised());
}

TEST(RemasterProfilesTest, WolfHasCompleteBaselinesAndIndependentNaturalWeapon)
{
    const auto first = CreateWolf();
    const auto second = CreateWolf({.Name = "Second wolf"});
    EXPECT_NE(first.Id(), second.Id());
    EXPECT_EQ(second.Name(), "Second wolf");
    EXPECT_EQ(first.Hitpoints().Maximum().Value(), 24);
    EXPECT_EQ(first.ArmorClass(), 15);
    EXPECT_EQ(first.PerceptionBonus(), 7);
    EXPECT_EQ(first.SaveBonus(ESavethrow::Fortitude), 6);
    EXPECT_EQ(first.SaveBonus(ESavethrow::Reflex), 9);
    EXPECT_EQ(first.SaveBonus(ESavethrow::Will), 5);
    EXPECT_EQ(first.SkillBonus(ESkill::Athletics), 6);
    EXPECT_EQ(first.SkillBonus(ESkill::Acrobatics), 7);
    EXPECT_EQ(first.SkillBonus(ESkill::Stealth), 7);
    EXPECT_EQ(first.SkillBonus(ESkill::Survival), 7);
    EXPECT_EQ(first.SpeedFeet(), 35);
    EXPECT_EQ(first.ZeroHpRule(), EZeroHpRule::Death);
    EXPECT_EQ(first.Languages().Size(), 0U);
    EXPECT_EQ(first.Senses().Size(), 2U);
    EXPECT_FALSE(first.HasClassDC());
    ASSERT_EQ(first.NaturalWeapons().Size(), 1U);
    const auto& jaws = *(*first.NaturalWeapons().begin());
    EXPECT_EQ(first.AttackBonus(jaws), 9);
    EXPECT_EQ(first.DamageModifier(jaws), 2);
    EXPECT_EQ(jaws.BaseDiceSize(), 6);
    EXPECT_EQ(jaws.DamageType(), EDamageType::Piercing);
    EXPECT_FALSE(jaws.Traits().Has(EWeaponTraitKind::Agile));
    EXPECT_EQ(ObjectFields(**second.NaturalWeapons().begin()), ObjectFields(jaws));
}

TEST(RemasterProfilesTest, GeneralTrainingAndHeritageChoicesHaveMechanicalEffects)
{
    THumanFighter1Options options;
    options.HeritageFeat = TFeatureSelection("remaster_profiles.Fleet");
    const auto fleet = CreateHumanFighter1(options);
    EXPECT_EQ(fleet.Hitpoints().Maximum().Value(), 20);
    EXPECT_EQ(fleet.SpeedFeet(), 30);
    options.AncestryFeat = TFeatureSelection("remaster_profiles.GeneralTraining");
    options.TrainingFeat = TFeatureSelection("abilities.Toughness");
    const auto both = CreateHumanFighter1(options);
    EXPECT_EQ(both.Hitpoints().Maximum().Value(), 21);
    EXPECT_EQ(both.SpeedFeet(), 30);
    EXPECT_EQ(both.Class().Features().Size(), 3U);
    options.TrainingFeat = TFeatureSelection("remaster_profiles.Fleet");
    EXPECT_FALSE(ValidateHumanFighter1(options).IsValid());
    EXPECT_THROW(CreateHumanFighter1(options), std::invalid_argument);
}

TEST(RemasterProfilesTest, DuplicateDisabledAndWrongPoolFeatsFailBeforeIdentityAllocation)
{
    const auto identity = PeekNextEntityId();
    THumanFighter1Options options;
    options.AmbitionFeat = TFeatureSelection("movement.SuddenCharge");
    EXPECT_FALSE(ValidateHumanFighter1(options).IsValid());
    EXPECT_THROW(CreateHumanFighter1(options), std::invalid_argument);
    options.ClassFeat = TFeatureSelection("abilities.ViciousSwing");
    EXPECT_TRUE(ValidateHumanFighter1(options).IsValid());
    options.ClassFeat = TFeatureSelection("abilities.Toughness");
    EXPECT_FALSE(ValidateHumanFighter1(options).IsValid());
    options = {};
    options.TrainingFeat = TFeatureSelection("remaster_profiles.Fleet");
    EXPECT_FALSE(ValidateHumanFighter1(options).IsValid());
    EXPECT_THROW(CreateHumanFighter1(options), std::invalid_argument);
    EXPECT_EQ(PeekNextEntityId(), identity);
}

TEST(RemasterProfilesTest, ArbitraryReadiedEquipmentUsesItsOwnStatsAndValidatesFreeHands)
{
    THumanFighter1Options options;
    options.Equipment = std::vector<TWeapon>{TWeapon::Create({
        .Name = "Declared polearm", .BaseDiceSize = 10,
        .DamageType = EDamageType::Piercing, .Category = EWeaponCategory::Martial,
        .Reach = 2, .HandsRequired = 2,
    })};
    EXPECT_FALSE(ValidateHumanFighter1(options).IsValid());
    options.ReadiedShield = TShield::Create({});
    const auto fighter = CreateHumanFighter1(options);
    ASSERT_EQ(fighter.Weapons().Size(), 1U);
    const auto& weapon = *(*fighter.Weapons().begin());
    EXPECT_EQ(fighter.AttackBonus(weapon), 9);
    EXPECT_EQ(fighter.DamageModifier(weapon), 4);
    EXPECT_EQ(weapon.ReachFeet(), 10);
    EXPECT_EQ(weapon.Features().Size(), 0U);
    EXPECT_EQ(fighter.Hands().Count(), 0);
    options.Equipment = std::vector<TWeapon>{};
    const auto unarmed = CreateHumanFighter1(options);
    EXPECT_EQ(unarmed.Weapons().Size(), 0U);
    EXPECT_EQ(unarmed.NaturalWeapons().Size(), 1U);
    EXPECT_EQ(unarmed.Hands().Count(), 2);
}

TEST(RemasterProfilesTest, GeneratedProfilesCollectTheirActualActionsAndReactions)
{
    const auto rules = CreateAbilityRules();
    for (const auto& creature : {CreateHumanFighter1(), CreateWolf()}) {
        const auto grants = CollectAbilities(creature, rules->Catalog);
        EXPECT_TRUE(std::ranges::any_of(grants.Grants(), [](const auto& grant) {
            return grant.DefinitionId == "abilities.UseStrike";
        }));
    }
    const auto fighter = CreateHumanFighter1();
    const auto grants = CollectAbilities(fighter, rules->Catalog);
    for (const auto* expected : {"abilities.UseViciousSwing", "movement.UseSuddenCharge",
         "movement.ReactiveStrike", "shield_actions.UseShieldBlock",
         "intimidation.UseIntimidatingGlare"}) {
        EXPECT_TRUE(std::ranges::any_of(grants.Grants(), [expected](const auto& grant) {
            return grant.DefinitionId == expected;
        })) << expected;
    }
}

}
