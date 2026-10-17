#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/inventory/check_modifiers.h>
#include <pf2e_engine/inventory/lore.h>
#include <pf2e_engine/inventory/statistics.h>
#include <pf2e_engine/transformation/transformator.h>

#include "../test_lib/mock_interaction_system.h"
#include "../test_lib/journal_setup.h"
#include "../test_lib/state_diff.h"

#include <gtest/gtest.h>

#include <limits>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <utility>

namespace {

TStatisticBaseline FixedTotal(int value)
{
    return TStatisticBaseline::Create({.IsFixed = true, .Value = value});
}

TProficiencyValue Trained()
{
    return TProficiencyValue::Create({.Rank = EProficiencyRank::Trained});
}

TAbilityScores AbilityScores(int strength, int dexterity, int constitution,
                           int intelligence, int wisdom, int charisma)
{
    return TAbilityScores::Create({
        .Strength = TAbilityScore::Create({.Value = strength}),
        .Dexterity = TAbilityScore::Create({.Value = dexterity}),
        .Constitution = TAbilityScore::Create({.Value = constitution}),
        .Intelligence = TAbilityScore::Create({.Value = intelligence}),
        .Wisdom = TAbilityScore::Create({.Value = wisdom}),
        .Charisma = TAbilityScore::Create({.Value = charisma}),
    });
}

TCreature::TInitialValues FighterValues()
{
    TCreature::TInitialValues values;
    values.Level = 1;
    values.Characteristic = AbilityScores(18, 14, 14, 10, 12, 10);
    values.Hitpoints = THitPoints::Create({
        .Maximum = Statistic::Create({.Base = 21}), .Current = 21,
    });
    values.Proficiency = TProficiency::Create({
        .Savethrow = std::map<ESavethrow, TProficiencyValue>{
            {ESavethrow::Fortitude, TProficiencyValue::Create({.Rank = EProficiencyRank::Expert})},
            {ESavethrow::Reflex, TProficiencyValue::Create({.Rank = EProficiencyRank::Expert})},
            {ESavethrow::Will, Trained()},
        },
        .Skill = std::map<ESkill, TProficiencyValue>{
            {ESkill::Athletics, Trained()}, {ESkill::Acrobatics, Trained()},
            {ESkill::Medicine, Trained()}, {ESkill::Survival, Trained()},
            {ESkill::Intimidation, Trained()},
        },
        .Perception = TProficiencyValue::Create({.Rank = EProficiencyRank::Expert}),
    });
    values.ClassDcProfile = TClassDCProfile::Create({
        .Enabled = true, .KeyAttribute = ECharacteristic::Strength, .Proficiency = Trained(),
    });
    TIdCollection<TLoreSkill> lore;
    lore.Add(TLoreSkill::Create({.Name = "Warfare", .Proficiency = Trained()}));
    lore.Add(TLoreSkill::Create({.Name = "Sailing"}));
    values.LoreSkills = std::move(lore);
    return values;
}

TCreature::TInitialValues WolfValues()
{
    TCreature::TInitialValues values;
    values.Level = 1;
    values.Characteristic = AbilityScores(14, 18, 12, 2, 14, 6);
    values.Hitpoints = THitPoints::Create({
        .Maximum = Statistic::Create({.Base = 24}), .Current = 24,
    });
    values.Proficiency = TProficiency::Create({
        .Skill = std::map<ESkill, TProficiencyValue>{
            {ESkill::Acrobatics, Trained()}, {ESkill::Athletics, Trained()},
            {ESkill::Stealth, Trained()}, {ESkill::Survival, Trained()},
        },
        .Perception = TProficiencyValue::Create({.Rank = EProficiencyRank::Expert}),
    });
    values.PerceptionBaseline = FixedTotal(7);
    values.SaveBaselines = std::map<ESavethrow, TStatisticBaseline>{
        {ESavethrow::Fortitude, FixedTotal(6)}, {ESavethrow::Reflex, FixedTotal(9)},
        {ESavethrow::Will, FixedTotal(5)},
    };
    values.SkillBaselines = std::map<ESkill, TStatisticBaseline>{
        {ESkill::Acrobatics, FixedTotal(7)}, {ESkill::Athletics, FixedTotal(6)},
        {ESkill::Stealth, FixedTotal(7)}, {ESkill::Survival, FixedTotal(7)},
    };
    return values;
}

const TLoreSkill& Lore(const TCreature& creature, std::size_t index)
{
    return *creature.LoreSkills().Find(TId<TLoreSkill>{index});
}

TAstNode Snapshot(const TCreature& creature, const TTransformator& journal)
{
    TAstContext context;
    context.RegisterIdentity(&creature, "creature");
    auto node = TAstNode::MakeObject("statistics");
    AddOwnedObject(node, "journal", journal, context);
    AddOwnedObject(node, "creature", creature, context);
    return node;
}

TEST(GeneratedStatisticsTest, FighterProfileUsesRanksAndChosenClassAttribute)
{
    const auto fighter = TCreature::Create(FighterValues());
    const auto snapshot = ObjectFields(fighter);
    EXPECT_EQ(fighter.PerceptionBonus(), 6);
    EXPECT_EQ(fighter.SaveBonus(ESavethrow::Fortitude), 7);
    EXPECT_EQ(fighter.SaveBonus(ESavethrow::Reflex), 7);
    EXPECT_EQ(fighter.SaveBonus(ESavethrow::Will), 4);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Athletics), 7);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Acrobatics), 5);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Medicine), 4);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Survival), 4);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Intimidation), 3);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Arcana), 0);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Stealth), 2);
    EXPECT_EQ(fighter.LoreBonus(Lore(fighter, 0)), 3);
    EXPECT_EQ(fighter.LoreBonus(Lore(fighter, 1)), 0);
    EXPECT_EQ(fighter.LoreDC(Lore(fighter, 0)), 13);
    EXPECT_TRUE(fighter.HasClassDC());
    EXPECT_EQ(fighter.ClassDC(), 17);
    EXPECT_EQ(fighter.PerceptionDC(), 16);
    EXPECT_EQ(fighter.SaveDC(ESavethrow::Will), 14);
    EXPECT_EQ(fighter.SkillDC(ESkill::Athletics), 17);
    EXPECT_TRUE(fighter.SkillBaselines().empty());
    EXPECT_TRUE(fighter.SaveBaselines().empty());
    EXPECT_EQ(ObjectFields(fighter), snapshot);

    auto values = FighterValues();
    values.ClassDcProfile = TClassDCProfile::Create({
        .Enabled = true, .KeyAttribute = ECharacteristic::Dexterity, .Proficiency = Trained(),
    });
    EXPECT_EQ(TCreature::Create(std::move(values)).ClassDC(), 15);
}

TEST(GeneratedStatisticsTest, WolfTotalsRemainIndependentFromRanksAndUnlistedSkills)
{
    const auto wolf = TCreature::Create(WolfValues());
    EXPECT_EQ(wolf.PerceptionBonus(), 7);
    EXPECT_EQ(wolf.SaveBonus(ESavethrow::Fortitude), 6);
    EXPECT_EQ(wolf.SaveBonus(ESavethrow::Reflex), 9);
    EXPECT_EQ(wolf.SaveBonus(ESavethrow::Will), 5);
    EXPECT_EQ(wolf.SkillBonus(ESkill::Acrobatics), 7);
    EXPECT_EQ(wolf.SkillBonus(ESkill::Athletics), 6);
    EXPECT_EQ(wolf.SkillBonus(ESkill::Stealth), 7);
    EXPECT_EQ(wolf.SkillBonus(ESkill::Survival), 7);
    EXPECT_EQ(wolf.SkillBonus(ESkill::Arcana), -4);
    EXPECT_EQ(wolf.SkillBonus(ESkill::Intimidation), -2);
    EXPECT_EQ(wolf.SkillBonus(ESkill::Medicine), 2);
    EXPECT_EQ(wolf.SkillDC(ESkill::Athletics), 16);
    EXPECT_EQ(wolf.PerceptionProficiency(), 5);
    EXPECT_EQ(wolf.SkillProficiency(ESkill::Athletics), 3);
    EXPECT_EQ(wolf.Proficiency().Skill().at(ESkill::Athletics).Rank(), EProficiencyRank::Trained);
    EXPECT_FALSE(wolf.HasClassDC());
    EXPECT_THROW(wolf.ClassDC(), std::runtime_error);
}

TEST(GeneratedStatisticsTest, LevelAttributesConditionsAndLoreReevaluateWithExactRollback)
{
    TMockInteractionSystem io;
    TTransformator journal(io);
    auto context = journal.ExecutionContext();
    auto fighter = TCreature::Create(FighterValues());
    const auto start = context.GetState();
    const auto before = Snapshot(fighter, journal);
    context.AssignGuarded(fighter.Level(), 4, TGuarded<TCreature>(fighter), "level");
    EXPECT_EQ(fighter.PerceptionBonus(), 9);
    EXPECT_EQ(fighter.SaveBonus(ESavethrow::Fortitude), 10);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Athletics), 10);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Stealth), 2);
    EXPECT_EQ(fighter.LoreBonus(Lore(fighter, 0)), 6);
    EXPECT_EQ(fighter.ClassDC(), 20);
    const auto level_state = context.GetState();
    const auto at_level = Snapshot(fighter, journal);
    context.AssignGuarded(fighter.Characteristic(), AbilityScores(20, 16, 18, 14, 16, 12),
                          TGuarded<TCreature>(fighter), "characteristic");
    EXPECT_EQ(fighter.PerceptionBonus(), 11);
    EXPECT_EQ(fighter.SaveBonus(ESavethrow::Fortitude), 12);
    EXPECT_EQ(fighter.SaveBonus(ESavethrow::Reflex), 11);
    EXPECT_EQ(fighter.SaveBonus(ESavethrow::Will), 9);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Athletics), 11);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Medicine), 9);
    EXPECT_EQ(fighter.LoreBonus(Lore(fighter, 0)), 8);
    EXPECT_EQ(fighter.LoreBonus(Lore(fighter, 1)), 2);
    EXPECT_EQ(fighter.ClassDC(), 21);
    SetCondition(journal, fighter, TConditionFrightened{.Value = 2});
    SetCondition(journal, fighter, TConditionProne{});
    EXPECT_EQ(fighter.PerceptionBonus(), 9);
    EXPECT_EQ(fighter.SaveBonus(ESavethrow::Fortitude), 10);
    EXPECT_EQ(fighter.SkillBonus(ESkill::Athletics), 9);
    EXPECT_EQ(fighter.LoreBonus(Lore(fighter, 0)), 6);
    EXPECT_EQ(fighter.ClassDC(), 19);
    context.Undo(level_state);
    EXPECT_EQ(Snapshot(fighter, journal), at_level);
    EXPECT_EQ(fighter.ClassDC(), 20);
    context.Undo(start);
    EXPECT_EQ(Snapshot(fighter, journal), before);
    EXPECT_EQ(fighter.LoreBonus(Lore(fighter, 0)), 3);
}

TEST(GeneratedStatisticsTest, NpcTotalsIgnoreLevelAndAttributesButStillApplyConditions)
{
    TMockInteractionSystem io;
    TTransformator journal(io);
    auto context = journal.ExecutionContext();
    auto wolf = TCreature::Create(WolfValues());
    const auto start = context.GetState();
    const auto before = Snapshot(wolf, journal);
    context.AssignGuarded(wolf.Level(), 20, TGuarded<TCreature>(wolf), "level");
    context.AssignGuarded(wolf.Characteristic(), AbilityScores(20, 20, 20, 20, 20, 20),
                          TGuarded<TCreature>(wolf), "characteristic");
    EXPECT_EQ(wolf.PerceptionBonus(), 7);
    EXPECT_EQ(wolf.SaveBonus(ESavethrow::Fortitude), 6);
    EXPECT_EQ(wolf.SkillBonus(ESkill::Athletics), 6);
    EXPECT_EQ(wolf.SkillBonus(ESkill::Arcana), 5);
    EXPECT_EQ(wolf.SkillProficiency(ESkill::Athletics), 22);
    SetCondition(journal, wolf, TConditionFrightened{.Value = 2});
    EXPECT_EQ(wolf.PerceptionBonus(), 5);
    EXPECT_EQ(wolf.SaveBonus(ESavethrow::Reflex), 7);
    EXPECT_EQ(wolf.SkillBonus(ESkill::Athletics), 4);
    EXPECT_EQ(wolf.SkillDC(ESkill::Athletics), 14);
    context.Undo(start);
    EXPECT_EQ(Snapshot(wolf, journal), before);
}

TEST(GeneratedStatisticsTest, ZeroNegativeTotalsAndFixedContributionsRemainDistinct)
{
    auto values = FighterValues();
    values.Proficiency = TProficiency::Create({
        .Perception = TProficiencyValue::Create({.IsFixed = true, .FixedBonus = 11}),
    });
    const auto contribution = TCreature::Create(values);
    EXPECT_EQ(contribution.PerceptionBonus(), 12);
    values.PerceptionBaseline = FixedTotal(11);
    values.SaveBaselines = std::map<ESavethrow, TStatisticBaseline>{{ESavethrow::Will, FixedTotal(-3)}};
    values.SkillBaselines = std::map<ESkill, TStatisticBaseline>{{ESkill::Athletics, FixedTotal(0)}};
    values.ClassDcProfile = TClassDCProfile::Create({.Enabled = true, .Baseline = FixedTotal(-2)});
    const auto complete = TCreature::Create(values);
    EXPECT_EQ(complete.PerceptionBonus(), 11);
    EXPECT_EQ(complete.SaveBonus(ESavethrow::Will), -3);
    EXPECT_EQ(complete.SaveDC(ESavethrow::Will), 7);
    EXPECT_EQ(complete.SkillBonus(ESkill::Athletics), 0);
    EXPECT_EQ(complete.SkillDC(ESkill::Athletics), 10);
    EXPECT_EQ(complete.ClassDC(), -2);
    values.PerceptionBaseline = FixedTotal(0);
    EXPECT_EQ(TCreature::Create(values).PerceptionBonus(), 0);
    values.PerceptionBaseline = TStatisticBaseline::Create({.IsFixed = false, .Value = 100});
    EXPECT_EQ(TCreature::Create(std::move(values)).PerceptionBonus(), 12);
}

TEST(GeneratedStatisticsTest, ExtraModifiersMergeWithFrightenedOnceForBonusesAndDcs)
{
    TMockInteractionSystem io;
    TTransformator journal(io);
    auto fighter = TCreature::Create(FighterValues());
    SetCondition(journal, fighter, TConditionFrightened{.Value = 2});
    const auto extra = TCheckModifiers::Create({
        .ItemBonus = 1, .StatusBonus = 3, .StatusPenalty = 1,
        .CircumstanceBonus = 2, .CircumstancePenalty = 1, .UntypedPenalty = 2,
    });
    const auto before = Snapshot(fighter, journal);
    const auto extra_snapshot = ObjectFields(extra);
    EXPECT_EQ(fighter.PerceptionBonus(), 4);
    EXPECT_EQ(fighter.PerceptionBonusWithModifiers(extra), 7);
    EXPECT_EQ(fighter.SaveBonusWithModifiers(ESavethrow::Fortitude, extra), 8);
    EXPECT_EQ(fighter.SkillBonusWithModifiers(ESkill::Athletics, extra), 8);
    EXPECT_EQ(fighter.LoreBonusWithModifiers(Lore(fighter, 0), extra), 4);
    EXPECT_EQ(fighter.ClassDCWithModifiers(extra), 18);
    EXPECT_EQ(fighter.PerceptionDCWithModifiers(extra), 17);
    EXPECT_EQ(fighter.SaveDCWithModifiers(ESavethrow::Fortitude, extra), 18);
    EXPECT_EQ(fighter.SkillDCWithModifiers(ESkill::Athletics, extra), 18);
    EXPECT_EQ(fighter.LoreDCWithModifiers(Lore(fighter, 0), extra), 14);
    const auto larger_penalty = TCheckModifiers::Create({.StatusPenalty = 4});
    EXPECT_EQ(fighter.PerceptionBonusWithModifiers(larger_penalty), 2);
    EXPECT_EQ(fighter.ClassDCWithModifiers(larger_penalty), 13);
    EXPECT_EQ(ObjectFields(extra), extra_snapshot);
    EXPECT_EQ(Snapshot(fighter, journal), before);
}

TEST(GeneratedStatisticsTest, FailedStatisticsQueriesArePureAndCheckIntegerBounds)
{
    TMockInteractionSystem io;
    TTransformator journal(io);
    auto context = journal.ExecutionContext();
    auto fighter = TCreature::Create(FighterValues());
    const auto before = Snapshot(fighter, journal);
    const auto invalid = TCheckModifiers::Create({.StatusPenalty = -1});
    EXPECT_THROW(fighter.SaveBonusWithModifiers(ESavethrow::Reflex, invalid), std::runtime_error);
    EXPECT_THROW(fighter.ClassDCWithModifiers(invalid), std::runtime_error);
    EXPECT_EQ(Snapshot(fighter, journal), before);
    const auto start = context.GetState();
    context.AssignGuarded(fighter.Level(), std::numeric_limits<int>::max(),
                          TGuarded<TCreature>(fighter), "level");
    const auto at_limit = Snapshot(fighter, journal);
    EXPECT_THROW(fighter.PerceptionBonus(), std::overflow_error);
    EXPECT_THROW(fighter.ClassDC(), std::overflow_error);
    EXPECT_EQ(Snapshot(fighter, journal), at_limit);
    context.Undo(start);
    EXPECT_EQ(Snapshot(fighter, journal), before);

    auto values = FighterValues();
    values.PerceptionBaseline = FixedTotal(std::numeric_limits<int>::max());
    const auto maximum = TCreature::Create(values);
    EXPECT_EQ(maximum.PerceptionBonus(), std::numeric_limits<int>::max());
    EXPECT_THROW(maximum.PerceptionDC(), std::overflow_error);
    EXPECT_THROW(maximum.PerceptionBonusWithModifiers(TCheckModifiers::Create({.ItemBonus = 1})), std::overflow_error);
    values.PerceptionBaseline = FixedTotal(std::numeric_limits<int>::min());
    const auto minimum = TCreature::Create(std::move(values));
    EXPECT_THROW(minimum.PerceptionBonusWithModifiers(TCheckModifiers::Create({.StatusPenalty = 1})), std::overflow_error);
}

}
