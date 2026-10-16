#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/inventory/generated_records.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

class TGeneratedRecordsTest : public TBattleFixture {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedRecordsRules(*rules);
        StartBattle(std::move(rules), 5, "generated_records.");
    }

    static TWeapon Weapon(int size = 4)
    {
        return TWeapon::Create({.Name = "Blade", .BaseDiceSize = size,
            .Features = Features({"generated_records.WeaponTraining"})});
    }

    TPlayer& Add(bool armed = false)
    {
        const int id = static_cast<int>(creatures_.size());
        TIdCollection<TWeapon> weapons;
        if (armed) {
            weapons.Add(Weapon());
            weapons.Add(Weapon(8));
        }
        return Join(TCreature::Create({
            .Name = "Actor " + std::to_string(id),
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = 100}), .Current = 100,
            }),
            .Weapons = std::move(weapons),
            .Features = Features({"generated_records.RootTraining"}),
            .Actions = TResource(10), .Reactions = TResource(3),
        }), id, {id, 0});
    }

    void Attack(int target = 1, bool suspend = false)
    {
        io_.ExpectName(0, "target", "Actor " + std::to_string(target));
        io_.ExpectValue(0, 1, suspend);
    }

    bool HasRecord(int actor = 0)
    {
        return battle_->History().Find(Player(actor), TRecordHitResult::kId).has_value();
    }

    static void SetDie(TTransformator& editor, TCreature& creature, int amount)
    {
        auto execution = editor.ExecutionContext();
        const auto weapon = execution.Project(creature.Weapons(), [](TIdCollection<TWeapon>& values) {
            return TGuarded<TWeapon>(*values.Find(TId<TWeapon>{0}));
        });
        const auto die = execution.Project(weapon, [](TWeapon& value) { return value.BaseDiceSize(); });
        execution.AssignGuarded(die, amount, weapon, "base_dice_size");
    }
};

TEST_F(TGeneratedRecordsTest, TerminalPublicationWaitsForChildCompletionAndFollowConsumesTheRecord)
{
    Add(true);
    Add();
    EXPECT_TRUE(Options("Follow").empty());
    Attack(1, true);
    const auto point = Pause([&] { Execute("BoundAttack"); });
    EXPECT_EQ(Read(0).Actions().Count(), 9);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 100);
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("Follow").empty());
    point.Resume();
    EXPECT_TRUE(HasRecord());
    EXPECT_EQ(Read(1).Hitpoints().Current(), 95);
    const auto follows = Options("Follow");
    ASSERT_EQ(follows.size(), 1U);
    EXPECT_TRUE(follows[0].Source.StartsWith(TAbilitySourcePath{}.Field("weapons").Entry(0)));
    io_.ExpectValue(0, 2);
    io_.ExpectValue(0, 1);
    EXPECT_EQ(Execute(follows[0]), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(0).Actions().Count(), 8);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 88);
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("Follow").empty());
}

TEST_F(TGeneratedRecordsTest, SelectedSourceAliasPublishesTheExactSecondSameNamedWeapon)
{
    Add(true);
    Add();
    io_.ExpectName(0, "target", "Actor 1");
    io_.ExpectName(0, "source", "Blade (2)");
    io_.ExpectValue(0, 1);
    EXPECT_EQ(Execute("SelectAttack"), EAbilityExecutionResult::Applied);
    const auto follows = Options("Follow");
    ASSERT_EQ(follows.size(), 1U);
    EXPECT_TRUE(follows[0].Source.StartsWith(TAbilitySourcePath{}.Field("weapons").Entry(1)));
    EXPECT_EQ(Read(1).Hitpoints().Current(), 91);
    EXPECT_EQ(Read(0).Actions().Count(), 9);
}

TEST_F(TGeneratedRecordsTest, SavedFollowOptionCannotRetargetAfterANewerSuccessfulAttack)
{
    Add(true);
    Add();
    Add();
    Attack();
    Execute("BoundAttack");
    const auto stale = Option("Follow");
    Attack(2);
    Execute("BoundAttack");
    const auto before = Snapshot();
    EXPECT_EQ(Execute(stale), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(Snapshot(), before);
    EXPECT_EQ(Read(0).Actions().Count(), 8);
    EXPECT_EQ(Execute("HistoricalFollow"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 95);
    EXPECT_EQ(Read(2).Hitpoints().Current(), 91);
}

TEST_F(TGeneratedRecordsTest, RejectedPreflightPreservesHistoryButPaidFailureAndAnotherActionInvalidateIt)
{
    Add(true);
    Add();
    Attack();
    Execute("BoundAttack");
    const auto published = journal_->CurrentState();
    const auto before = Snapshot();
    EXPECT_TRUE(Options("Unavailable").empty());
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    editor.ExecutionContext().AssignGuarded(Creature(0).Level(), 6,
        TGuarded<TCreature>(Creature(0)), "level");
    const auto stale = Option("Unavailable");
    editor.Undo(edit);
    EXPECT_EQ(Execute(stale), EAbilityExecutionResult::Unavailable);
    EXPECT_TRUE(HasRecord());
    EXPECT_EQ(Snapshot(), before);
    EXPECT_THROW(Execute("Fail"), std::runtime_error);
    EXPECT_FALSE(HasRecord());
    EXPECT_EQ(Read(0).Actions().Count(), 8);
    journal_->Undo(published);
    EXPECT_EQ(Snapshot(), before);
    Execute("Wait");
    EXPECT_FALSE(HasRecord());
    journal_->Undo(published);
    io_.ExpectName(0, "target", "Actor 1");
    Execute("Miss");
    EXPECT_TRUE(HasRecord());
    EXPECT_TRUE(Options("Follow").empty());
}

TEST_F(TGeneratedRecordsTest, OtherActorsActionsAndReactionsLeaveThePublishersRecordAvailable)
{
    Add(true);
    Add();
    Attack();
    Execute("BoundAttack");
    Execute("Wait", 1);
    EXPECT_EQ(Options("Follow").size(), 1U);
    io_.ExpectName(1, "target", "Actor 1");
    io_.ExpectName(1, "reaction", "Intervene");
    Execute("OpenWindow", 1);
    EXPECT_EQ(Read(1).Reactions().Count(), 2);
    EXPECT_EQ(Options("Follow").size(), 1U);
}

TEST_F(TGeneratedRecordsTest, OwnReactionInvalidatesHistoryAndLateParentPublicationCannotReplaceIt)
{
    Add(true);
    Add();
    Attack();
    Execute("BoundAttack");
    io_.ExpectName(1, "target", "Actor 0");
    io_.ExpectName(0, "reaction", "Intervene");
    Execute("OpenWindow", 1);
    EXPECT_FALSE(HasRecord());
    io_.ExpectName(0, "target", "Actor 1");
    io_.ExpectName(0, "reaction", "Intervene");
    EXPECT_EQ(Execute("PublishAfterWindow"), EAbilityExecutionResult::Applied);
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("Follow").empty());
    EXPECT_EQ(Read(0).Reactions().Count(), 1);
}

TEST_F(TGeneratedRecordsTest, AcceptedFollowRetainsInputsThroughTwoSuspensionsAndExactReplay)
{
    Add(true);
    Add();
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    Attack();
    Execute("BoundAttack");
    io_.ExpectValue(0, 2, true);
    const auto first = Pause([&] { Execute("Follow"); });
    EXPECT_FALSE(HasRecord());
    EXPECT_EQ(Read(0).Actions().Count(), 8);
    io_.ExpectValue(0, 1, true);
    const auto second = Pause([&] { first.Resume(); });
    second.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 88);
    const auto completed = Snapshot();
    second.Revert(*journal_);
    second.Resume();
    EXPECT_EQ(Snapshot(), completed);
    first.Revert(*journal_);
    io_.ExpectValue(0, 1);
    first.Resume();
    EXPECT_EQ(Snapshot(), completed);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedRecordsTest, RemovingAndReplacingTheSourceCannotSatisfyTheRecordedIdentity)
{
    Add(true);
    Add();
    Attack();
    Execute("BoundAttack");
    const auto option = Option("Follow");
    TTransformator editor(io_);
    const auto initial = editor.CurrentState();
    auto weapons = Read(0).Weapons();
    weapons.Remove(TId<TWeapon>{0});
    weapons.Add(Weapon());
    editor.ExecutionContext().AssignGuarded(Creature(0).Weapons(), std::move(weapons),
        TGuarded<TCreature>(Creature(0)), "weapons");
    EXPECT_TRUE(Options("Follow").empty());
    EXPECT_TRUE(Options("HistoricalFollow").empty());
    EXPECT_EQ(Execute(option), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(Read(0).Actions().Count(), 9);
    editor.Undo(initial);
    EXPECT_EQ(Options("Follow").size(), 1U);
    EXPECT_EQ(Options("HistoricalFollow").size(), 1U);
}

TEST_F(TGeneratedRecordsTest, HistoricalSourceSnapshotSurvivesAttributeChangesAcrossAChildPause)
{
    Add(true);
    Add();
    Attack();
    Execute("BoundAttack");
    io_.ExpectValue(0, 2);
    io_.ExpectValue(0, 1, true);
    const auto point = Pause([&] { Execute("Follow"); });
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    SetDie(editor, Creature(0), 12);
    point.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 88);
    EXPECT_EQ(Read(0).Weapons().Find(TId<TWeapon>{0})->BaseDiceSize(), 12);
    editor.Undo(edit);
}

TEST_F(TGeneratedRecordsTest, RemovingHistoricalSourceDuringAChildPauseRejectsEffectsUntilRestored)
{
    Add(true);
    Add();
    Attack();
    Execute("BoundAttack");
    io_.ExpectValue(0, 2);
    io_.ExpectValue(0, 1, true);
    const auto point = Pause([&] { Execute("Follow"); });
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    auto weapons = Read(0).Weapons();
    weapons.Remove(TId<TWeapon>{0});
    editor.ExecutionContext().AssignGuarded(Creature(0).Weapons(), std::move(weapons),
        TGuarded<TCreature>(Creature(0)), "weapons");
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 95);
    EXPECT_EQ(Read(0).Actions().Count(), 8);
    editor.Undo(edit);
    point.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 88);
    const auto completed = Snapshot();
    point.Revert(*journal_);
    point.Resume();
    EXPECT_EQ(Snapshot(), completed);
}

TEST_F(TGeneratedRecordsTest, HistoricalInputDoesNotDependOnTheOldAttackGrant)
{
    Add(true);
    Add();
    Attack();
    Execute("BoundAttack");
    auto execution = journal_->ExecutionContext();
    auto weapon = execution.Project(Creature(0).Weapons(), [](TIdCollection<TWeapon>& values) {
        return TGuarded<TWeapon>(*values.Find(TId<TWeapon>{0}));
    });
    auto features = execution.Project(weapon, [](TWeapon& value) { return value.Features(); });
    execution.AssignGuarded(features, TIdCollection<TFeatureSelection>{}, weapon, "features");
    EXPECT_EQ(Options("BoundAttack").size(), 1U);
    EXPECT_TRUE(Options("Follow").empty());
    EXPECT_EQ(Execute("HistoricalFollow"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 91);
}

TEST_F(TGeneratedRecordsTest, SuccessfulSubordinateActivityDoesNotPublishARootActionRecord)
{
    Add(true);
    Add();
    Attack();
    EXPECT_EQ(Execute("Subordinate"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 95);
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("Follow").empty());
    EXPECT_EQ(Read(0).Actions().Count(), 9);
}

}
