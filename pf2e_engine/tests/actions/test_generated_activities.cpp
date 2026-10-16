#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/inventory/generated_activities.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

class TGeneratedActivitiesTest : public TBattleFixture {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedActivitiesRules(*rules);
        StartBattle(std::move(rules), 4, "generated_activities.");
    }

    TPlayer& Add(std::string name, bool trained, int level = 1, bool armed = false)
    {
        TIdCollection<TFeatureSelection> features;
        if (trained) {
            features.Add(TFeatureSelection("generated_activities.ActivityTraining"));
        }
        TIdCollection<TWeapon> weapons;
        if (armed) {
            weapons.Add(CreateActivityBlade());
        }
        const int id = static_cast<int>(creatures_.size());
        return Join(TCreature::Create({
            .Level = level,
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = 100}), .Current = 100,
            }),
            .Weapons = std::move(weapons),
            .Features = std::move(features),
            .Actions = TResource(3),
            .Hands = TResource(2),
        }), id, {id, 0}, std::move(name));
    }

    TPlayer& Actor(int level = 1, bool armed = false)
    {
        return Add("Actor", true, level, armed);
    }

    TPlayer& Target()
    {
        return Add("Target", false);
    }
};

TEST_F(TGeneratedActivitiesTest, NestedTypedReturnsResumeEachParentOnceAndReplayIndependentFrames)
{
    auto& actor = Actor();
    auto& target = Target();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectName(0, {}, "Target");
    io_.ExpectValue(0, 2, true);
    const auto amount_point = Pause([&] { Execute("NestedActivities", actor.GetId()); });
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100);

    io_.ExpectValue(0, EActivityDecision::Accepted, true);
    const auto outcome_point = Pause([&] { amount_point.Resume(); });
    io_.ExpectValue(0, 1, true);
    const auto extra_point = Pause([&] { outcome_point.Resume(); });
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 97);
    io_.ExpectValue(0, 2, true);
    const auto parent_point = Pause([&] { extra_point.Resume(); });
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 93);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
    parent_point.Resume();
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 7);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    const auto completed = Snapshot();
    parent_point.Revert(*journal_);
    parent_point.Resume();
    EXPECT_EQ(Snapshot(), completed);

    extra_point.Revert(*journal_);
    io_.ExpectValue(0, 4);
    extra_point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 93);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 13);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);

    amount_point.Revert(*journal_);
    io_.ExpectValue(0, EActivityDecision::Declined);
    io_.ExpectValue(0, 2);
    amount_point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 6);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedActivitiesTest, ImmediateNestedCompletionsReturnWithoutCallingResumeChannels)
{
    auto& actor = Actor();
    auto& target = Target();
    io_.ExpectName(0, {}, "Target");
    io_.ExpectValue(0, 5);
    io_.ExpectValue(0, EActivityDecision::Accepted);
    io_.ExpectValue(0, 3);
    io_.ExpectValue(0, 4);
    EXPECT_EQ(Execute("NestedActivities", actor.GetId()), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 85);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 25);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(io_.Requests(), 5u);
    EXPECT_EQ(io_.MaxDepth(), 1);
}

TEST_F(TGeneratedActivitiesTest, ThousandsOfImmediateCallsKeepTheLoopStackBounded)
{
    auto& actor = Actor(8192);
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    EXPECT_EQ(Execute("ImmediateActivities", actor.GetId()), EAbilityExecutionResult::Applied);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 8192);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(io_.Requests(), 0u);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedActivitiesTest, DiscardedTypedResultsContinueOnceOnImmediateAndSuspendedPaths)
{
    auto& actor = Actor();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectValue(0, 2);
    EXPECT_EQ(Execute("DiscardActivityResult", actor.GetId()), EAbilityExecutionResult::Applied);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    io_.ExpectValue(0, 5, true);
    const auto point = Pause([&] { Execute("DiscardActivityResult", actor.GetId()); });
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 1);
    point.Resume();
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 4);
    const auto completed = Snapshot();
    point.Revert(*journal_);
    point.Resume();
    EXPECT_EQ(Snapshot(), completed);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedActivitiesTest, SourceSnapshotSurvivesNestedSuspensionAndNextCallUsesFreshValues)
{
    auto& actor = Actor(1, true);
    auto& target = Target();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectName(0, {}, "Target");
    io_.ExpectValue(0, 0, true);
    const auto point = Pause([&] { Execute("WeaponActivities", actor.GetId()); });

    TTransformator editor(io_);
    auto execution = editor.ExecutionContext();
    const auto edit_start = execution.GetState();
    auto weapon = execution.Project(actor.GetCreature()->Weapons(), [](TIdCollection<TWeapon>& values) {
        return TGuarded<TWeapon>(*values.Find(TId<TWeapon>{0}));
    });
    auto die = execution.Project(weapon, [](TWeapon& value) { return value.BaseDiceSize(); });
    execution.AssignGuarded(die, 12, weapon, "base_dice_size");

    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 96);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 5);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    point.Revert(*journal_);
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 96);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 5);
    io_.ExpectName(0, {}, "Target");
    io_.ExpectValue(0, 1);
    EXPECT_EQ(Execute("WeaponActivities", actor.GetId()), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 83);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 14);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 1);
    journal_->Undo(initial);
    execution.Undo(edit_start);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedActivitiesTest, RemovedSourceRejectsChildResumeAndUndoRestoresReplay)
{
    auto& actor = Actor(1, true);
    auto& target = Target();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectName(0, {}, "Target");
    io_.ExpectValue(0, 0, true);
    const auto point = Pause([&] { Execute("WeaponActivities", actor.GetId()); });
    TTransformator editor(io_);
    auto execution = editor.ExecutionContext();
    const auto edit_start = execution.GetState();
    auto weapons = actor.GetCreature()->Weapons().Get();
    weapons.Remove(TId<TWeapon>{0});
    execution.AssignGuarded(actor.GetCreature()->Weapons(), std::move(weapons),
                            TGuarded<TCreature>(*actor.GetCreature()), "weapons");
    const auto paused = Snapshot();
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Snapshot(), paused);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    execution.Undo(edit_start);
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 96);
    point.Revert(*journal_);
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 96);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedActivitiesTest, ChildFailureKeepsPaidAndCompletedEffectsUntilCallerUndo)
{
    auto& actor = Actor();
    auto& target = Target();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectName(0, {}, "Target");
    io_.ExpectValue(0, 1, true);
    const auto point = Pause([&] { Execute("FailingActivity", actor.GetId()); });
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 97);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    const auto paused = Snapshot();
    EXPECT_THROW(point.Resume(), std::runtime_error);
    EXPECT_EQ(Snapshot(), paused);
    point.Revert(*journal_);
    EXPECT_THROW(point.Resume(), std::runtime_error);
    EXPECT_EQ(Snapshot(), paused);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedActivitiesTest, ActivityEntryRejectsForeignAndReadonlyEntityArgumentsBeforeEffects)
{
    auto& actor = Actor();
    auto& target = Target();
    const auto context = Context(actor.GetId());
    auto foreign_creature = TCreature::Create({});
    TPlayer foreign(&foreign_creature, TPlayerTeam{0}, TPlayerId{9}, "Foreign", "");
    const auto before = Snapshot();
    int resumed = 0;
    std::function<void(int)> on_resume = [&](int) { ++resumed; };
    EXPECT_THROW(RunGeneratedActivitiesEntryProbe(context, BindActionEntity<TCreature>(foreign),
                                                  on_resume), TAbilityUnavailable);
    EXPECT_THROW(RunGeneratedActivitiesEntryProbe(context,
        BindActionEntity<TCreature>(std::as_const(target)), on_resume), std::logic_error);
    EXPECT_THROW(RunGeneratedActivitiesEntryProbe(context, BindActionEntity<TCreature>(target), {}),
                 std::invalid_argument);
    EXPECT_EQ(Snapshot(), before);
    EXPECT_EQ(RunGeneratedActivitiesEntryProbe(context, BindActionEntity<TCreature>(target),
                                               on_resume), 7);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
    EXPECT_EQ(resumed, 0);
    context->battle = nullptr;
    EXPECT_EQ(RunGeneratedActivitiesIncrement(context, 9, on_resume), 10);
    EXPECT_EQ(resumed, 0);
}

TEST_F(TGeneratedActivitiesTest, ActorAnchorRejectsContextReplacementAndAcceptsRestoredIdentity)
{
    auto& actor = Actor();
    auto& other = Target();
    const auto context = Context(actor.GetId());
    const auto self = ActionSelf<TCreature>(context);
    EXPECT_NO_THROW(ValidateActionActorBinding(context, self.ReadOnly().View()));
    EXPECT_THROW(ValidateActionActorBinding(context, BindActionEntity<TCreature>(other).View()),
                 TAbilityUnavailable);
    EXPECT_THROW(ValidateActionActorBinding(context, TGeneratedEntityView{}), TAbilityUnavailable);
    auto foreign_creature = TCreature::Create({});
    TPlayer foreign(&foreign_creature, TPlayerTeam{0}, TPlayerId{0}, "Foreign", "");
    EXPECT_THROW(ValidateActionActorBinding(context, BindActionEntity<TCreature>(foreign).View()),
                 TAbilityUnavailable);

    io_.ExpectValue(0, 2, true);
    const auto point = Pause([&] { Execute("DiscardActivityResult", actor.GetId()); });
    const auto registry = last_context_->game_object_registry;
    last_context_->game_object_registry = Context(other.GetId())->game_object_registry;
    const auto paused = Snapshot();
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Snapshot(), paused);
    last_context_->game_object_registry = registry;
    point.Resume();
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    EXPECT_EQ(other.GetCreature()->Hands()->Count(), 2);
    point.Revert(*journal_);
    point.Resume();
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
}

TEST_F(TGeneratedActivitiesTest, ActivityRevalidatesUnusedEntityArgumentsBeforeResumedEffects)
{
    auto& actor = Actor();
    auto& target = Target();
    const auto context = Context(actor.GetId());
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    int resumed = 0;
    std::function<void(int)> on_resume = [&](int value) {
        EXPECT_EQ(value, 7);
        ++resumed;
    };
    io_.ExpectValue(0, 7, true);
    const auto point = Pause([&] {
        RunGeneratedActivitiesPausedEntryProbe(context, BindActionEntity<TCreature>(target),
                                               on_resume);
    });
    context->battle = nullptr;
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
    EXPECT_EQ(resumed, 0);
    context->battle = battle_.get();
    auto moved = std::move(*target.GetCreature());
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
    EXPECT_EQ(resumed, 0);
    *target.GetCreature() = std::move(moved);
    point.Resume();
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    EXPECT_EQ(resumed, 1);
    point.Revert(*journal_);
    point.Resume();
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    EXPECT_EQ(resumed, 2);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedActivitiesTest, PositionResultKeepsCapturedOriginAndJournalsMovementAcrossReplay)
{
    auto& actor = Actor();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectValue(0, TPosition{0, 1}, true);
    const auto point = Pause([&] { Execute("PositionActivity", actor.GetId()); });
    EXPECT_EQ(actor.GetPosition(), (TPosition{0, 0}));
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    TTransformator editor(io_);
    const auto edit_start = editor.CurrentState();
    editor.MovePlayer(&actor, {1, 1});
    const auto relocated = Snapshot();
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Snapshot(), relocated);
    editor.Undo(edit_start);

    point.Resume();
    EXPECT_EQ(actor.GetPosition(), (TPosition{0, 1}));
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 1);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    const auto completed = Snapshot();
    point.Revert(*journal_);
    EXPECT_EQ(actor.GetPosition(), (TPosition{0, 0}));
    point.Resume();
    EXPECT_EQ(Snapshot(), completed);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

}
