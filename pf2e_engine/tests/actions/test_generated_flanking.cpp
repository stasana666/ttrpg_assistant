#include <pf2e_engine/actions/feature_event.h>
#include <pf2e_engine/inventory/combat_geometry.h>
#include <pf2e_engine/inventory/flanking.h>
#include <pf2e_engine/inventory/remaster_profiles.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

class TGeneratedFlankingTest : public TGeneratedCombatFixture {
protected:
    static TCreature Fighter(std::string name, int reach = 1)
    {
        THumanFighter1Options options;
        options.Name = std::move(name);
        options.Equipment = std::vector<TWeapon>{TWeapon::Create({
            .Name = "Reach weapon", .BaseDiceSize = 8,
            .DamageType = EDamageType::Piercing, .Category = EWeaponCategory::Martial,
            .Reach = reach, .HandsRequired = 2,
        })};
        options.ReadiedShield = TShield::Create({});
        return CreateHumanFighter1(options);
    }

    TCheckModifiers Modifiers(int actor = 0, int target = 1)
    {
        auto result = TCheckModifiers::Create({});
        for (const auto& contribution : DispatchFeatureEvent<TEventMeleeTargetModifiers>(
                Context(actor), BindActionEntity<TCreature>(Player(actor)),
                TEventMeleeTargetModifiers::TArguments{BindActionEntity<TCreature>(Player(target))})) {
            result = result.Merge(contribution.Value);
        }
        return result;
    }

    void OppositeWolves()
    {
        Join(CreateWolf({.Name = "Attacker"}), 1, {1, 2});
        Join(CreateHumanFighter1({.Name = "Target"}), 0, {2, 2});
        Join(CreateWolf({.Name = "Ally"}), 1, {3, 2});
    }

    void Strike(const std::string& weapon = "Jaws", bool pause_type = false)
    {
        io_.ExpectName(0, "target", "Target");
        io_.ExpectName(0, "source", weapon);
        if (weapon == "Longsword") {
            io_.ExpectValue(0, EDamageType::Piercing, pause_type);
        }
    }
};

TEST_F(TGeneratedFlankingTest, GeometryUsesOppositeClosedEdgesAndDeclaredReachExceptions)
{
    const auto geometry = TCombatGeometry::Create({});
    EXPECT_TRUE(geometry.Flanks(3, 4, 5, 4, 4, 4));
    EXPECT_TRUE(geometry.Flanks(4, 3, 4, 5, 4, 4));
    EXPECT_TRUE(geometry.Flanks(3, 3, 5, 5, 4, 4));
    EXPECT_TRUE(geometry.Flanks(1, 3, 7, 5, 4, 4));
    EXPECT_TRUE(geometry.Flanks(3, 4, 6, 5, 4, 4));
    EXPECT_FALSE(geometry.Flanks(3, 4, 4, 5, 4, 4));
    EXPECT_FALSE(geometry.Flanks(3, 3, 5, 3, 4, 4));
    EXPECT_FALSE(geometry.Flanks(3, 3, 3, 5, 4, 4));
    EXPECT_FALSE(geometry.Flanks(4, 4, 5, 4, 4, 4));
    EXPECT_TRUE(geometry.InReach(0, 0, 1, 1, 1));
    EXPECT_TRUE(geometry.InReach(0, 0, 2, 2, 2));
    EXPECT_FALSE(geometry.InReach(0, 0, 3, 3, 3));
    EXPECT_TRUE(geometry.InReach(0, 0, 3, 2, 4));
}

TEST_F(TGeneratedFlankingTest, TwoWolvesFlankForTheirStrikeWithoutPersistentAcOrPackDamage)
{
    OppositeWolves();
    BeginTurn();
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    EXPECT_EQ(Read(2).Actions().Count(), 0);
    EXPECT_EQ(Read(2).Reactions().Count(), 0);
    EXPECT_TRUE(Read(2).CanAct());
    const auto modifier = Modifiers();
    EXPECT_EQ(modifier.CircumstancePenalty(), 2);
    EXPECT_EQ(Read(1).ArmorClass(), 15);
    EXPECT_EQ(Read(1).ArmorClassWithModifiers(modifier), 13);
    EXPECT_EQ(Snapshot(), before);
    Strike();
    rng_.ExpectCall(20, 4);
    rng_.ExpectCall(6, 2);
    EXPECT_EQ(Execute("abilities.UseStrike"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 17);
    EXPECT_EQ(Read(1).ArmorClass(), 15);
    EXPECT_TRUE(Read(1).Conditions().empty());
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    EXPECT_EQ(Read(0).AttacksMade(), 1);
    EXPECT_EQ(Read(2).Actions().Count(), 0);
    EXPECT_EQ(Read(2).Hitpoints().Current(), 24);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedFlankingTest, AnUnconsciousAllyCannotFlankButSpentResourcesDoNotPreventIt)
{
    OppositeWolves();
    BeginTurn();
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 2);
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    editor.ExecutionContext().SetVariant(Creature(2).Conditions(), TCondition(TConditionUnconscious{}),
        TGuarded<TCreature>(Creature(2)), "conditions");
    EXPECT_FALSE(Read(2).CanAct());
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 0);
    Strike();
    rng_.ExpectCall(20, 4);
    Execute("abilities.UseStrike");
    EXPECT_EQ(Read(1).Hitpoints().Current(), 21);
    editor.Undo(edit);
    EXPECT_TRUE(Read(2).CanAct());
    EXPECT_EQ(Read(2).Actions().Count(), 0);
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 2);
}

TEST_F(TGeneratedFlankingTest, ProneShieldAndFrightenedMergeWithFlankingByModifierType)
{
    OppositeWolves();
    BeginTurn();
    auto execution = journal_->ExecutionContext();
    execution.Invoke(Creature(1).Shield(), &TShield::Raise);
    execution.Invoke(TGuarded<TCreature>(Creature(1)), &TCreature::ApplyFrightened, 2);
    execution.SetVariant(Creature(1).Conditions(), TCondition(TConditionProne{}),
        TGuarded<TCreature>(Creature(1)), "conditions");
    const auto modifier = Modifiers();
    EXPECT_EQ(modifier.CircumstancePenalty(), 2);
    EXPECT_EQ(Read(1).ArmorClass(), 13);
    EXPECT_EQ(Read(1).ArmorClassWithModifiers(modifier), 13);
    const auto explicit_modifiers = TCheckModifiers::Create({
        .StatusPenalty = 1, .CircumstanceBonus = 1, .CircumstancePenalty = 4});
    EXPECT_EQ(Read(1).ArmorClassWithModifiers(modifier.Merge(explicit_modifiers)), 11);
    Strike();
    rng_.ExpectCall(20, 3);
    Execute("abilities.UseStrike");
    EXPECT_EQ(Read(1).Hitpoints().Current(), 21);
    EXPECT_EQ(Read(1).Shield().Hitpoints(), 20);
    EXPECT_EQ(Read(1).ConditionPenalty(), 2);
    EXPECT_EQ(Read(1).ArmorClass(), 13);
}

TEST_F(TGeneratedFlankingTest, FlankerPositionIsReadAfterAVersatileChoiceAndReplaysThroughUndo)
{
    Join(CreateHumanFighter1({.Name = "Attacker"}), 0, {1, 2});
    Join(CreateWolf({.Name = "Target"}), 1, {2, 2});
    Join(CreateWolf({.Name = "Ally"}), 0, {3, 2});
    BeginTurn();
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 2);
    Strike("Longsword", true);
    const auto point = Pause([&] { Execute("abilities.UseStrike"); });
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    editor.MovePlayer(&Player(2), {2, 3});
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 0);
    rng_.ExpectCall(20, 4);
    point.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 24);
    point.Revert(*journal_);
    editor.Undo(edit);
    rng_.ExpectCall(20, 4);
    rng_.ExpectCall(8, 2);
    point.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 18);
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    const auto completed = Snapshot();
    point.Revert(*journal_);
    rng_.ExpectCall(20, 4);
    rng_.ExpectCall(8, 2);
    point.Resume();
    EXPECT_EQ(Snapshot(), completed);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedFlankingTest, AllyThreatUsesHeldAndNaturalProfilesWithoutABodyReachFallback)
{
    Join(CreateWolf({.Name = "Attacker"}), 1, {1, 3});
    Join(CreateHumanFighter1({.Name = "Target"}), 0, {2, 3});
    Join(Fighter("Ally", 2), 1, {4, 3});
    EXPECT_EQ(Read(2).Hands().Count(), 0);
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 2);
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    auto execution = editor.ExecutionContext();
    execution.AssignGuarded(Creature(2).Weapons(), TIdCollection<TWeapon>{},
        TGuarded<TCreature>(Creature(2)), "weapons");
    EXPECT_EQ(Read(2).MaxWeaponReach(), 1);
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 0);
    execution.AssignGuarded(Creature(2).BodyReach(), 3, TGuarded<TCreature>(Creature(2)), "body_reach");
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 0);
    TIdCollection<TWeapon> natural;
    natural.Add(TWeapon::Create({.Name = "Long natural attack", .BaseDiceSize = 6,
        .DamageType = EDamageType::Piercing, .Category = EWeaponCategory::Unarmed,
        .Reach = 2, .HandsRequired = 0}));
    execution.AssignGuarded(Creature(2).NaturalWeapons(), std::move(natural),
        TGuarded<TCreature>(Creature(2)), "natural_weapons");
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 2);
    execution.AssignGuarded(Creature(2).NaturalWeapons(), TIdCollection<TWeapon>{},
        TGuarded<TCreature>(Creature(2)), "natural_weapons");
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 0);
    editor.Undo(edit);
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 2);
}

TEST_F(TGeneratedFlankingTest, FifteenFootReachRejectsTheThirdDiagonalBeforePayment)
{
    Join(Fighter("Attacker", 3), 0, {1, 1});
    Join(CreateWolf({.Name = "Target"}), 1, {4, 4});
    BeginTurn();
    const auto before = Snapshot();
    EXPECT_FALSE(Available("abilities.UseStrike"));
    EXPECT_FALSE(Available("abilities.UseViciousSwing"));
    EXPECT_EQ(Read(0).Actions().Count(), 3);
    EXPECT_EQ(io_.Requests(), 0U);
    EXPECT_EQ(Snapshot(), before);
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    editor.MovePlayer(&Player(1), {4, 1});
    auto options = battle_->AvailableActions(Player(0));
    const auto found = std::ranges::find(options, std::string("abilities.UseStrike"), &TAbilityOption::DefinitionId);
    ASSERT_NE(found, options.end());
    const auto stale = *found;
    editor.Undo(edit);
    EXPECT_EQ(ExecuteAbility(stale, Context(), Player(0),
        CollectAbilities(Read(0), rules_->Catalog), rules_->Executions), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedFlankingTest, TenFootReachIncludesASecondDiagonalStrike)
{
    Join(Fighter("Attacker", 2), 0, {1, 1});
    Join(CreateWolf({.Name = "Target"}), 1, {3, 3});
    BeginTurn();
    ASSERT_TRUE(Available("abilities.UseStrike"));
    Strike("Reach weapon");
    rng_.ExpectCall(20, 6);
    rng_.ExpectCall(8, 2);
    Execute("abilities.UseStrike");
    EXPECT_EQ(Read(1).Hitpoints().Current(), 18);
    EXPECT_EQ(Read(0).Actions().Count(), 2);
}

TEST_F(TGeneratedFlankingTest, FriendlyTargetsAdjacentSidesAndUnsupportedFootprintsDoNotFlank)
{
    OppositeWolves();
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 2);
    EXPECT_EQ(Modifiers(0, 2).CircumstancePenalty(), 0);
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    editor.MovePlayer(&Player(2), {2, 3});
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 0);
    editor.Undo(edit);
    auto execution = editor.ExecutionContext();
    execution.AssignGuarded(Creature(1).Size(), ECreatureSize::Large,
        TGuarded<TCreature>(Creature(1)), "size");
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 0);
    editor.Undo(edit);
    execution.AssignGuarded(Creature(2).Size(), ECreatureSize::Tiny,
        TGuarded<TCreature>(Creature(2)), "size");
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 0);
    editor.Undo(edit);
    execution.AssignGuarded(Creature(0).Size(), ECreatureSize::Small,
        TGuarded<TCreature>(Creature(0)), "size");
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 2);
    editor.Undo(edit);
}

}
