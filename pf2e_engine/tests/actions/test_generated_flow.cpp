#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/inventory/generated_flow.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

class TGeneratedFlowTest : public TBattleFixture {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedFlowRules(*rules);
        StartBattle(std::move(rules), 8, "generated_flow.");
    }

    TPlayer& Add(std::string name, TPosition position, int team, int level, bool trained)
    {
        TIdCollection<TFeatureSelection> features;
        if (trained) {
            features.Add(TFeatureSelection("generated_flow.FlowTraining"));
        }
        return Join(TCreature::Create({
            .Level = level,
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = 100}),
                .Current = 100,
            }),
            .Features = features,
            .Actions = TResource(3),
            .Hands = TResource(2),
        }), team, position, std::move(name));
    }

    TPlayer& Actor(int level = 1)
    {
        return Add("Actor", {0, 0}, 0, level, true);
    }

    TPlayer& Target(std::string name = "Target", int level = 1, TPosition position = {7, 7})
    {
        return Add(std::move(name), position, 1, level, false);
    }
};

TEST_F(TGeneratedFlowTest, NestedBranchChoicesPreserveOuterLocalsAndReplayWithoutReroll)
{
    auto& actor = Actor();
    auto& target = Target("Target", 2);
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectValue(0, EFlowDecision::Continue);
    rng_.ExpectCall(6, 4);
    io_.ExpectName(0, {}, "Target", true);
    auto target_point = Pause([&]() { Execute("BranchFlow", actor.GetId()); });
    EXPECT_EQ(rng_.RemainingCalls(), 0u);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100);

    io_.ExpectValue(0, EFlowDecision::Continue, true);
    auto boost_point = Pause([&]() { target_point.Resume(); });
    const auto paused = Snapshot();
    boost_point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 93);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 9);
    boost_point.Revert(*journal_);
    EXPECT_EQ(Snapshot(), paused);
    boost_point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 93);

    target_point.Revert(*journal_);
    io_.ExpectValue(0, EFlowDecision::Finish);
    target_point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 95);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 7);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedFlowTest, EarlyReturnSkipsBranchDiceTargetsAndFollowingStatements)
{
    auto& actor = Actor();
    auto& target = Target();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectValue(0, EFlowDecision::Finish, true);
    auto point = Pause([&]() { Execute("BranchFlow", actor.GetId()); });
    point.Resume();
    EXPECT_EQ(io_.Requests(), 1u);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedFlowTest, LoopBindingsSurviveRepeatedSuspensionsAndEarlierSavepointReplay)
{
    auto& actor = Actor(2);
    auto& first = Target("First");
    auto& second = Target("Second", 1, {6, 7});
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    rng_.ExpectCall(6, 2);
    io_.ExpectValue(0, EFlowDecision::Continue);
    io_.ExpectName(0, {}, "First", true);
    auto first_point = Pause([&]() { Execute("LoopFlow", actor.GetId()); });
    io_.ExpectValue(0, EFlowDecision::Continue);
    io_.ExpectName(0, {}, "Second", true);
    auto second_point = Pause([&]() { first_point.Resume(); });
    EXPECT_EQ(first.GetCreature()->Hitpoints()->Current(), 98);
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 100);
    const auto second_pause = Snapshot();
    second_point.Resume();
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 96);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 5);
    second_point.Revert(*journal_);
    EXPECT_EQ(Snapshot(), second_pause);
    second_point.Resume();
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 96);

    first_point.Revert(*journal_);
    io_.ExpectValue(0, EFlowDecision::Continue);
    io_.ExpectName(0, {}, "Second", true);
    auto replayed = Pause([&]() { first_point.Resume(); });
    replayed.Resume();
    EXPECT_EQ(first.GetCreature()->Hitpoints()->Current(), 98);
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 96);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 5);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(rng_.RemainingCalls(), 0u);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedFlowTest, ReturnFromLoopKeepsCompletedIterationsAndSkipsTheNextTarget)
{
    auto& actor = Actor(3);
    auto& target = Target();
    rng_.ExpectCall(6, 2);
    io_.ExpectValue(0, EFlowDecision::Continue);
    io_.ExpectName(0, {}, "Target");
    io_.ExpectValue(0, EFlowDecision::Finish, true);
    auto point = Pause([&]() { Execute("LoopFlow", actor.GetId()); });
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 98);
    point.Resume();
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 5);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(io_.Requests(), 3u);
}

TEST_F(TGeneratedFlowTest, ThousandsOfImmediateChoicesDoNotGrowTheCallbackStack)
{
    constexpr int kIterations = 4096;
    auto& actor = Actor(kIterations);
    Target();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    for (int index = 0; index < kIterations; ++index) {
        io_.ExpectValue(0, EFlowDecision::Continue);
    }
    EXPECT_EQ(Execute("ImmediateFlow", actor.GetId()), EAbilityExecutionResult::Applied);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), kIterations);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(io_.Requests(), static_cast<size_t>(kIterations));
    EXPECT_LE(io_.MaxDepth(), 2);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedFlowTest, EmptyLoopSkipsChoicesAndRunsTheFollowingStatement)
{
    auto& actor = Actor(0);
    Target();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    EXPECT_EQ(Execute("ImmediateFlow", actor.GetId()), EAbilityExecutionResult::Applied);
    EXPECT_EQ(io_.Requests(), 0u);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 0);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedFlowTest, FrameNamesCannotCollideAcrossBlocksOrWithSynchronousLoopBindings)
{
    auto& actor = Actor();
    Target();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectValue(0, EFlowDecision::Continue, true);
    auto point = Pause([&]() { Execute("NameCollisionFlow", actor.GetId()); });
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 17);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    const auto paused = Snapshot();
    point.Resume();
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 24);
    point.Revert(*journal_);
    EXPECT_EQ(Snapshot(), paused);
    point.Resume();
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 24);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(io_.Requests(), 1u);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedFlowTest, AuthoredTypesDoNotCollideWithGeneratedContinuationInternals)
{
    auto& actor = Actor();
    Target();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectValue(0, EFlowDecision::Continue, true);
    auto point = Pause([&]() { Execute("TypeCollisionFlow", actor.GetId()); });
    const auto paused = Snapshot();
    point.Resume();
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 21);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    point.Revert(*journal_);
    EXPECT_EQ(Snapshot(), paused);
    point.Resume();
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 21);
    EXPECT_EQ(io_.Requests(), 1u);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedFlowTest, PositionChoicesRejectNewOccupantsAndReplayBothMovesWithExactUndo)
{
    auto& actor = Actor();
    Target();
    auto& blocker = Target("Blocker", 1, {7, 0});
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectValue(0, TPosition{1, 0}, true);
    auto first_point = Pause([&]() { Execute("TraverseFlow", actor.GetId()); });
    TTransformator editor(io_);
    const auto edit_start = editor.CurrentState();
    editor.MovePlayer(&blocker, {1, 0});
    EXPECT_THROW(first_point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(actor.GetPosition(), (TPosition{0, 0}));
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
    editor.Undo(edit_start);

    editor.MovePlayer(&actor, {1, 1});
    EXPECT_THROW(first_point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(actor.GetPosition(), (TPosition{1, 1}));
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
    editor.Undo(edit_start);

    io_.ExpectValue(0, TPosition{2, 1}, true);
    auto second_point = Pause([&]() { first_point.Resume(); });
    EXPECT_EQ(actor.GetPosition(), (TPosition{1, 0}));
    const auto second_pause = Snapshot();
    second_point.Resume();
    EXPECT_EQ(actor.GetPosition(), (TPosition{2, 1}));
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    second_point.Revert(*journal_);
    EXPECT_EQ(Snapshot(), second_pause);
    second_point.Resume();
    EXPECT_EQ(actor.GetPosition(), (TPosition{2, 1}));

    first_point.Revert(*journal_);
    io_.ExpectValue(0, TPosition{0, 1});
    first_point.Resume();
    EXPECT_EQ(actor.GetPosition(), (TPosition{0, 1}));
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 1);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedFlowTest, VariantReplacementErasureAndResourcesShareTheRollbackJournal)
{
    auto& actor = Actor();
    Target();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectValue(0, EFlowDecision::Continue, true);
    auto point = Pause([&]() { Execute("MutationFlow", actor.GetId()); });
    EXPECT_EQ(actor.GetCreature()->ConditionPenalty(), 5);
    EXPECT_FALSE(actor.GetCreature()->Conditions()->Has(EConditionKind::Prone));
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 1);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 7);
    const auto paused = Snapshot();
    point.Resume();
    EXPECT_TRUE(actor.GetCreature()->Conditions()->empty());
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    point.Revert(*journal_);
    EXPECT_EQ(Snapshot(), paused);
    point.Resume();
    EXPECT_TRUE(actor.GetCreature()->Conditions()->empty());
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedFlowTest, InvalidSpendRejectsWithoutPartialResourceMutationAndCanBeUndone)
{
    auto& actor = Actor(3);
    Target();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    EXPECT_THROW(Execute("InvalidSpendFlow", actor.GetId()), std::invalid_argument);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
    EXPECT_EQ(actor.GetCreature()->ConditionPenalty(), 4);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedFlowTest, NegativeSpendRejectsWithoutTurningItIntoAResourceGain)
{
    auto& actor = Actor(-1);
    Target();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    EXPECT_THROW(Execute("InvalidSpendFlow", actor.GetId()), std::invalid_argument);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedFlowTest, NegativeSetAndOverflowLeaveTheFailedStatementUnapplied)
{
    auto& actor = Actor();
    Target();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    EXPECT_THROW(Execute("NegativeSetFlow", actor.GetId()), std::invalid_argument);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);

    EXPECT_THROW(Execute("OverflowFlow", actor.GetId()), std::overflow_error);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), std::numeric_limits<int>::max());
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

}
