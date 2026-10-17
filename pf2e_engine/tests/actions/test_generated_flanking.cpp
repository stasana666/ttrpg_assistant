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
        BeginTurn();
    }

    void Strike(std::vector<TPick> choices, std::vector<std::pair<int, int>> rolls, int damage = 0)
    {
        std::map<std::string, TField> changes = {{"Attacker.actions.count", 2}, {"Attacker.attacks_made", 1}};
        if (damage > 0) {
            changes.emplace("Target.hitpoints.current", Read(1).Hitpoints().Current() - damage);
        }
        choices.insert(choices.begin(), "Target");
        Act("abilities.UseStrike", 0, {.Choose = std::move(choices), .Rolls = std::move(rolls), .Changes = changes});
    }

    void SetSize(int player, ECreatureSize size)
    {
        journal_->ExecutionContext().AssignGuarded(Creature(player).Size(), size, TGuarded<TCreature>(Creature(player)),
                                                   "size");
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

TEST_F(TGeneratedFlankingTest, TwoWolvesFlankForTheirStrikeWithoutAPersistentPenalty)
{
    OppositeWolves();
    EXPECT_TRUE(Read(2).CanAct());
    const auto modifier = Modifiers();
    EXPECT_EQ(modifier.CircumstancePenalty(), 2);
    EXPECT_EQ(Read(1).ArmorClass(), 15);
    EXPECT_EQ(Read(1).ArmorClassWithModifiers(modifier), 13);
    Strike({"Jaws"}, {{20, 4}, {6, 2}}, 4);
    EXPECT_EQ(Read(1).ArmorClass(), 15);
}

TEST_F(TGeneratedFlankingTest, AnUnconsciousAllyCannotFlankButSpentResourcesDoNotMatter)
{
    OppositeWolves();
    EXPECT_EQ(Read(2).Actions().Count(), 0);
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 2);
    journal_->ExecutionContext().SetVariant(Creature(2).Conditions(), TCondition(TConditionUnconscious{}),
        TGuarded<TCreature>(Creature(2)), "conditions");
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 0);
    Strike({"Jaws"}, {{20, 4}});
}

TEST_F(TGeneratedFlankingTest, ProneShieldAndFrightenedMergeWithFlankingByModifierType)
{
    OppositeWolves();
    auto execution = journal_->ExecutionContext();
    execution.Invoke(Creature(1).Shield(), &TShield::Raise);
    execution.Invoke(TGuarded<TCreature>(Creature(1)), &TCreature::ApplyFrightened, 2);
    execution.SetVariant(Creature(1).Conditions(), TCondition(TConditionProne{}), TGuarded<TCreature>(Creature(1)),
                         "conditions");
    const auto modifier = Modifiers();
    EXPECT_EQ(modifier.CircumstancePenalty(), 2);
    EXPECT_EQ(Read(1).ArmorClass(), 13);
    EXPECT_EQ(Read(1).ArmorClassWithModifiers(modifier), 13);
    const auto explicit_modifiers = TCheckModifiers::Create({
        .StatusPenalty = 1, .CircumstanceBonus = 1, .CircumstancePenalty = 4});
    EXPECT_EQ(Read(1).ArmorClassWithModifiers(modifier.Merge(explicit_modifiers)), 11);
    Strike({"Jaws"}, {{20, 3}});
}

TEST_F(TGeneratedFlankingTest, FlankingIsReadWhenTheAttackIsRolled)
{
    Join(CreateHumanFighter1({.Name = "Attacker"}), 0, {1, 2});
    Join(CreateWolf({.Name = "Target"}), 1, {2, 2});
    Join(CreateWolf({.Name = "Ally"}), 0, {2, 3});
    BeginTurn();
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 0);
    Check(Option("abilities.UseStrike"), {.Choose = {"Target", "Longsword", TPick::Value(EDamageType::Piercing)},
        .Rolls = {{20, 4}}, .Changes = {{"Attacker.actions.count", 2}, {"Attacker.attacks_made", 1}}});
    io_.ExpectName(0, "target", "Target");
    io_.ExpectName(0, "source", "Longsword");
    io_.ExpectValue(0, EDamageType::Piercing, true);
    const auto point = Pause([&] { Execute("abilities.UseStrike"); });
    journal_->MovePlayer(&Player(2), {3, 2});
    rng_.ExpectCall(20, 4);
    rng_.ExpectCall(8, 2);
    point.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 18);
}

TEST_F(TGeneratedFlankingTest, AllyThreatUsesHeldAndNaturalWeaponsButNotBodyReach)
{
    Join(CreateWolf({.Name = "Attacker"}), 1, {1, 3});
    Join(CreateHumanFighter1({.Name = "Target"}), 0, {2, 3});
    Join(Fighter("Ally", 2), 1, {4, 3});
    EXPECT_EQ(Read(2).Hands().Count(), 0);
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 2);
    auto execution = journal_->ExecutionContext();
    execution.AssignGuarded(Creature(2).Weapons(), TIdCollection<TWeapon>{}, TGuarded<TCreature>(Creature(2)),
                            "weapons");
    EXPECT_EQ(Read(2).MaxWeaponReach(), 1);
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 0);
    execution.AssignGuarded(Creature(2).BodyReach(), 3, TGuarded<TCreature>(Creature(2)), "body_reach");
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 0);
    TIdCollection<TWeapon> natural;
    natural.Add(TWeapon::Create({.Name = "Long natural attack", .BaseDiceSize = 6, .DamageType = EDamageType::Piercing,
        .Category = EWeaponCategory::Unarmed, .Reach = 2, .HandsRequired = 0}));
    execution.AssignGuarded(Creature(2).NaturalWeapons(), std::move(natural), TGuarded<TCreature>(Creature(2)),
                            "natural_weapons");
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 2);
}

TEST_F(TGeneratedFlankingTest, FifteenFootReachDoesNotIncludeTheThirdDiagonal)
{
    Join(Fighter("Attacker", 3), 0, {1, 1});
    Join(CreateWolf({.Name = "Target"}), 1, {4, 1});
    BeginTurn();
    const auto stale = Option("abilities.UseStrike");
    journal_->MovePlayer(&Player(1), {4, 4});
    EXPECT_FALSE(Available("abilities.UseStrike"));
    EXPECT_FALSE(Available("abilities.UseViciousSwing"));
    Act(stale, {.Result = EAbilityExecutionResult::Unavailable});
}

TEST_F(TGeneratedFlankingTest, TenFootReachIncludesTheSecondDiagonal)
{
    Join(Fighter("Attacker", 2), 0, {1, 1});
    Join(CreateWolf({.Name = "Target"}), 1, {3, 3});
    BeginTurn();
    Strike({"Reach weapon"}, {{20, 6}, {8, 2}}, 6);
}

TEST_F(TGeneratedFlankingTest, FriendlyTargetsAdjacentSidesAndUnsupportedSizesDoNotFlank)
{
    OppositeWolves();
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 2);
    EXPECT_EQ(Modifiers(0, 2).CircumstancePenalty(), 0);
    const auto start = journal_->CurrentState();
    journal_->MovePlayer(&Player(2), {2, 3});
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 0);
    journal_->Undo(start);
    SetSize(1, ECreatureSize::Large);
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 0);
    journal_->Undo(start);
    SetSize(2, ECreatureSize::Tiny);
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 0);
    journal_->Undo(start);
    SetSize(0, ECreatureSize::Small);
    EXPECT_EQ(Modifiers().CircumstancePenalty(), 2);
}

}
