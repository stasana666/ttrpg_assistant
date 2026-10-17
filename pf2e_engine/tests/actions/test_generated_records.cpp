#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/inventory/generated_records.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>

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

    void Hit(int target = 1)
    {
        io_.ExpectName(0, "target", "Actor " + std::to_string(target));
        io_.ExpectValue(0, 1);
        Execute("BoundAttack");
    }

    void RemoveFirstWeapon(TTransformator& editor, bool replace)
    {
        auto weapons = Read(0).Weapons();
        weapons.Remove(TId<TWeapon>{0});
        if (replace) {
            weapons.Add(Weapon());
        }
        editor.ExecutionContext().AssignGuarded(Creature(0).Weapons(), std::move(weapons),
                                                TGuarded<TCreature>(Creature(0)), "weapons");
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

TEST_F(TGeneratedRecordsTest, PublicationWaitsForTheChildAndFollowConsumesTheRecord)
{
    Add(true);
    Add();
    EXPECT_TRUE(Options("Follow").empty());
    const auto unpublished = TPick::Value(1).WhenOffered([this](const TAlternatives&) { EXPECT_FALSE(HasRecord()); });
    Act("BoundAttack", 0, {.Choose = {"Actor 1", unpublished},
        .Changes = {{"Actor 1.hitpoints.current", 95}, {"Actor 0.actions.count", 9}}});
    EXPECT_TRUE(HasRecord());
    const auto follows = Options("Follow");
    ASSERT_EQ(follows.size(), 1U);
    EXPECT_TRUE(follows[0].Source.StartsWith(TAbilitySourcePath{}.Field("weapons").Entry(0)));
    Act(follows[0], {.Choose = {TPick::Value(2), TPick::Value(1)},
        .Changes = {{"Actor 1.hitpoints.current", 88}, {"Actor 0.actions.count", 8}}});
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("Follow").empty());
}

TEST_F(TGeneratedRecordsTest, SelectedSourcePublishesTheExactSecondSameNamedWeapon)
{
    Add(true);
    Add();
    Act("SelectAttack", 0, {.Choose = {"Actor 1", "Blade (2)", TPick::Value(1)},
        .Changes = {{"Actor 1.hitpoints.current", 91}, {"Actor 0.actions.count", 9}}});
    const auto follows = Options("Follow");
    ASSERT_EQ(follows.size(), 1U);
    EXPECT_TRUE(follows[0].Source.StartsWith(TAbilitySourcePath{}.Field("weapons").Entry(1)));
}

TEST_F(TGeneratedRecordsTest, ASavedFollowCannotRetargetAfterANewerHit)
{
    Add(true);
    Add();
    Add();
    Hit();
    const auto stale = Option("Follow");
    Hit(2);
    Act(stale, {.Result = EAbilityExecutionResult::Unavailable});
    Act("HistoricalFollow", 0, {.Changes = {{"Actor 2.hitpoints.current", 91}, {"Actor 0.actions.count", 7}}});
}

TEST_F(TGeneratedRecordsTest, RejectedPreflightKeepsTheRecordButAnyAcceptedOwnActionReplacesIt)
{
    Add(true);
    Add();
    Hit();
    const auto published = journal_->CurrentState();
    EXPECT_TRUE(Options("Unavailable").empty());
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    editor.ExecutionContext().AssignGuarded(Creature(0).Level(), 6, TGuarded<TCreature>(Creature(0)), "level");
    const auto stale = Option("Unavailable");
    editor.Undo(edit);
    Act(stale, {.Result = EAbilityExecutionResult::Unavailable});
    EXPECT_TRUE(HasRecord());
    EXPECT_THROW(Execute("Fail"), std::runtime_error);
    EXPECT_FALSE(HasRecord());
    journal_->Undo(published);
    Execute("Wait");
    EXPECT_FALSE(HasRecord());
    journal_->Undo(published);
    io_.ExpectName(0, "target", "Actor 1");
    Execute("Miss");
    EXPECT_TRUE(HasRecord());
    EXPECT_TRUE(Options("Follow").empty());
}

TEST_F(TGeneratedRecordsTest, OtherActorsActionsAndReactionsKeepThePublishersRecord)
{
    Add(true);
    Add();
    Hit();
    Execute("Wait", 1);
    EXPECT_EQ(Options("Follow").size(), 1U);
    io_.ExpectName(1, "target", "Actor 1");
    io_.ExpectName(1, "reaction", "Intervene");
    Execute("OpenWindow", 1);
    EXPECT_EQ(Options("Follow").size(), 1U);
}

TEST_F(TGeneratedRecordsTest, OwnReactionReplacesTheRecordAndALateParentPublicationCannotRestoreIt)
{
    Add(true);
    Add();
    Hit();
    io_.ExpectName(1, "target", "Actor 0");
    io_.ExpectName(0, "reaction", "Intervene");
    Execute("OpenWindow", 1);
    EXPECT_FALSE(HasRecord());
    io_.ExpectName(0, "target", "Actor 1");
    io_.ExpectName(0, "reaction", "Intervene");
    EXPECT_EQ(Execute("PublishAfterWindow"), EAbilityExecutionResult::Applied);
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("Follow").empty());
}

TEST_F(TGeneratedRecordsTest, AReplacedSourceCannotSatisfyTheRecordedIdentity)
{
    Add(true);
    Add();
    Hit();
    const auto option = Option("Follow");
    TTransformator editor(io_);
    const auto initial = editor.CurrentState();
    RemoveFirstWeapon(editor, true);
    EXPECT_TRUE(Options("Follow").empty());
    EXPECT_TRUE(Options("HistoricalFollow").empty());
    Act(option, {.Result = EAbilityExecutionResult::Unavailable});
    editor.Undo(initial);
    EXPECT_EQ(Options("Follow").size(), 1U);
    EXPECT_EQ(Options("HistoricalFollow").size(), 1U);
}

TEST_F(TGeneratedRecordsTest, HistoricalSourceSnapshotIgnoresLaterAttributeChanges)
{
    Add(true);
    Add();
    Hit();
    io_.ExpectValue(0, 2);
    io_.ExpectValue(0, 1, true);
    const auto point = Pause([&] { Execute("Follow"); });
    TTransformator editor(io_);
    SetDie(editor, Creature(0), 12);
    point.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 88);
}

TEST_F(TGeneratedRecordsTest, HistoricalInputDoesNotNeedTheOldAttackGrant)
{
    Add(true);
    Add();
    Hit();
    auto execution = journal_->ExecutionContext();
    auto weapon = execution.Project(Creature(0).Weapons(), [](TIdCollection<TWeapon>& values) {
        return TGuarded<TWeapon>(*values.Find(TId<TWeapon>{0}));
    });
    auto features = execution.Project(weapon, [](TWeapon& value) { return value.Features(); });
    execution.AssignGuarded(features, TIdCollection<TFeatureSelection>{}, weapon, "features");
    EXPECT_EQ(Options("BoundAttack").size(), 1U);
    EXPECT_TRUE(Options("Follow").empty());
    Act("HistoricalFollow", 0, {.Changes = {{"Actor 1.hitpoints.current", 91}, {"Actor 0.actions.count", 8}}});
}

TEST_F(TGeneratedRecordsTest, ASubordinateActivityDoesNotPublishARootRecord)
{
    Add(true);
    Add();
    Act("Subordinate", 0, {.Choose = {"Actor 1", TPick::Value(1)},
        .Changes = {{"Actor 1.hitpoints.current", 95}, {"Actor 0.actions.count", 9}}});
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("Follow").empty());
}

}
