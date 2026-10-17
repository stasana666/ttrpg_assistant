#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/inventory/creature_description.h>
#include <pf2e_engine/transformation/transformator.h>

#include "../test_lib/mock_interaction_system.h"
#include "../test_lib/state_diff.h"

#include <gtest/gtest.h>

#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

TAstNode Snapshot(const TCreature& creature, const TTransformator& journal)
{
    TAstContext context;
    context.RegisterIdentity(&creature, "described_creature");
    auto node = TAstNode::MakeObject("creature_description");
    AddOwnedObject(node, "creature", creature, context);
    AddOwnedObject(node, "journal", journal, context);
    return node;
}

TEST(CreatureDescriptionTest, NamesTraitsLanguagesAndSensePrecision)
{
    const auto default_sense = TSense::Create({});
    EXPECT_EQ(default_sense.Kind(), ESenseKind::Vision);
    EXPECT_EQ(default_sense.Name(), "");
    EXPECT_EQ(default_sense.Precision(), ESensePrecision::Precise);
    EXPECT_EQ(default_sense.RangeFeet(), 0);
    TIdCollection<TLanguage> languages;
    languages.Add(TLanguage::Create({.Name = "Common"}));
    languages.Add(TLanguage::Create({.Name = "Local Sign"}));
    TIdCollection<TSense> senses;
    senses.Add(TSense::Create({.Kind = ESenseKind::LowLightVision, .Name = "low-light vision"}));
    senses.Add(TSense::Create({
        .Kind = ESenseKind::Scent, .Name = "scent", .RangeFeet = 30, .Precision = ESensePrecision::Imprecise,
    }));
    const auto creature = TCreature::Create({
        .Name = "Test scout", .Traits = std::set<ECreatureTrait>{ECreatureTrait::Human, ECreatureTrait::Humanoid},
        .Languages = std::move(languages), .Senses = std::move(senses), .Level = 1,
        .Race = TRace::Create({.Name = "Human", .Hitpoints = 8}),
        .Class = TClass::Create({.Name = "Fighter", .Hitpoints = 10}),
    });
    EXPECT_EQ(creature.Name(), "Test scout");
    EXPECT_EQ(creature.Race().Name(), "Human");
    EXPECT_EQ(creature.Class().Name(), "Fighter");
    EXPECT_EQ(creature.Traits(), (std::set<ECreatureTrait>{ECreatureTrait::Human, ECreatureTrait::Humanoid}));
    ASSERT_EQ(creature.Languages().Size(), 2u);
    EXPECT_EQ(creature.Languages().Find(TId<TLanguage>{1})->Name(), "Local Sign");
    ASSERT_EQ(creature.Senses().Size(), 2u);
    const auto& vision = *creature.Senses().Find(TId<TSense>{0});
    const auto& scent = *creature.Senses().Find(TId<TSense>{1});
    EXPECT_EQ(vision.Kind(), ESenseKind::LowLightVision);
    EXPECT_EQ(vision.Precision(), ESensePrecision::Precise);
    EXPECT_EQ(vision.RangeFeet(), 0);
    EXPECT_EQ(scent.Kind(), ESenseKind::Scent);
    EXPECT_EQ(scent.Precision(), ESensePrecision::Imprecise);
    EXPECT_EQ(scent.RangeFeet(), 30);
}

TEST(CreatureDescriptionTest, DescriptionChangesUndoExactly)
{
    TIdCollection<TLanguage> languages;
    languages.Add(TLanguage::Create({.Name = "Common"}));
    const auto removed = languages.Add(TLanguage::Create({.Name = "Old dialect"}));
    languages.Remove(removed);
    const auto dialect = languages.Add(TLanguage::Create({.Name = "River dialect"}));
    auto creature = TCreature::Create({.Name = "Speaker", .Languages = std::move(languages)});
    EXPECT_EQ(std::as_const(creature).Languages().NextId().Value, 3u);
    EXPECT_EQ(std::as_const(creature).Languages().Find(removed), nullptr);
    ASSERT_NE(std::as_const(creature).Languages().Find(dialect), nullptr);
    EXPECT_EQ(std::as_const(creature).Languages().Find(dialect)->Name(), "River dialect");

    TMockInteractionSystem io;
    TTransformator journal(io);
    auto execution = journal.ExecutionContext();
    const auto checkpoint = journal.CurrentState();
    const auto before = Snapshot(creature, journal);
    execution.AssignGuarded(creature.Name(), std::string("Renamed"),
                            TGuarded<TCreature>(creature), "name");
    execution.AssignGuarded(creature.Languages(), TIdCollection<TLanguage>{},
                            TGuarded<TCreature>(creature), "languages");
    EXPECT_EQ(creature.Languages()->Size(), 0u);
    EXPECT_NE(Snapshot(creature, journal), before);
    journal.Undo(checkpoint);
    EXPECT_EQ(Snapshot(creature, journal), before);
}

TEST(CreatureDescriptionTest, AdvancedWeaponUsesItsOwnTrainedRankAndCurrentLevel)
{
    const auto weapon = TWeapon::Create({
        .Name = "Declared advanced weapon", .BaseDiceSize = 8,
        .DamageType = EDamageType::Slashing, .Category = EWeaponCategory::Advanced,
    });
    auto creature = TCreature::Create({
        .Level = 1,
        .Characteristic = TAbilityScores::Create({.Strength = TAbilityScore::Create({.Value = 18})}),
        .Proficiency = TProficiency::Create({.WeaponCategory = std::map<EWeaponCategory, TProficiencyValue>{
            {EWeaponCategory::Martial, TProficiencyValue::Create({.Rank = EProficiencyRank::Expert})},
            {EWeaponCategory::Advanced, TProficiencyValue::Create({.Rank = EProficiencyRank::Trained})},
        }}),
    });
    EXPECT_EQ(creature.WeaponProficiency(EWeaponCategory::Advanced), 3);
    EXPECT_EQ(creature.AttackBonus(weapon), 7);
    EXPECT_EQ(creature.WeaponProficiency(EWeaponCategory::Martial), 5);
    TMockInteractionSystem io;
    TTransformator journal(io);
    const auto checkpoint = journal.CurrentState();
    const auto before = Snapshot(creature, journal);
    auto execution = journal.ExecutionContext();
    execution.AssignGuarded(creature.Level(), 4, TGuarded<TCreature>(creature), "level");
    EXPECT_EQ(creature.AttackBonus(weapon), 10);
    journal.Undo(checkpoint);
    EXPECT_EQ(Snapshot(creature, journal), before);
    EXPECT_EQ(creature.AttackBonus(weapon), 7);
}

TEST(CreatureDescriptionTest, DisplayFeetDeriveFromExistingCellsWithoutDuplicateStorage)
{
    auto creature = TCreature::Create({.Movement = 5});
    const auto weapon = TWeapon::Create({.Reach = 2});
    const auto before = ObjectFields(creature);
    EXPECT_EQ(creature.SpeedFeet(), 25);
    EXPECT_EQ(weapon.ReachFeet(), 10);
    EXPECT_FALSE(before.contains("speed_feet"));
    EXPECT_FALSE(ObjectFields(weapon).contains("reach_feet"));
    EXPECT_EQ(ObjectFields(creature), before);
    TMockInteractionSystem io;
    TTransformator journal(io);
    const auto checkpoint = journal.CurrentState();
    auto execution = journal.ExecutionContext();
    execution.AssignGuarded(creature.Movement(), 7, TGuarded<TCreature>(creature), "movement");
    EXPECT_EQ(creature.SpeedFeet(), 35);
    journal.Undo(checkpoint);
    EXPECT_EQ(ObjectFields(creature), before);
    const auto excessive_speed = TCreature::Create({.Movement = std::numeric_limits<int>::max()});
    const auto excessive_reach = TWeapon::Create({.Reach = std::numeric_limits<int>::max()});
    EXPECT_THROW(excessive_speed.SpeedFeet(), std::overflow_error);
    EXPECT_THROW(excessive_reach.ReachFeet(), std::overflow_error);
}

}
