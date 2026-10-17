#include <pf2e_engine/inventory/generated_flow.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

const TPick kContinue = TPick::Value(EFlowDecision::Continue);
const TPick kFinish = TPick::Value(EFlowDecision::Finish);

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
        return Join(TCreature::Create({
            .Level = level,
            .Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 100}), .Current = 100}),
            .Features = trained ? Features({"generated_flow.FlowTraining"}) : Features({}),
            .Actions = TResource(3),
            .Hands = TResource(2),
        }), team, position, std::move(name));
    }

    TPlayer& Actor(int level = 1) { return Add("Actor", {0, 0}, 0, level, true); }

    TPlayer& Target(std::string name = "Target", int level = 1, TPosition position = {7, 7})
    {
        return Add(std::move(name), position, 1, level, false);
    }
};

TEST_F(TGeneratedFlowTest, NestedBranchesKeepOuterLocalsAndChooseTheirPath)
{
    Actor();
    Target("Target", 2);
    const auto option = Option("BranchFlow");
    Check(option, {.Choose = {kContinue, "Target", kContinue}, .Rolls = {{6, 4}},
        .Changes = {{"Target.hitpoints.current", 93}, {"Actor.hands.count", 9}, {"Actor.actions.count", 2}}});
    Check(option, {.Choose = {kContinue, "Target", kFinish}, .Rolls = {{6, 4}},
        .Changes = {{"Target.hitpoints.current", 95}, {"Actor.hands.count", 7}, {"Actor.actions.count", 2}}});
    Act(option, {.Choose = {kFinish}, .Changes = {{"Actor.actions.count", 2}}});
}

TEST_F(TGeneratedFlowTest, LoopBindingsSurviveSuspensionAndReturnSkipsTheRestOfTheLoop)
{
    Actor(2);
    Target("First");
    Target("Second", 1, {6, 7});
    Check(Option("LoopFlow"), {.Choose = {kContinue, "First", kContinue, "Second"}, .Rolls = {{6, 2}},
        .Changes = {{"First.hitpoints.current", 98}, {"Second.hitpoints.current", 96}, {"Actor.hands.count", 5},
                    {"Actor.actions.count", 2}}});
    Act(Option("LoopFlow"), {.Choose = {kContinue, "First", kFinish}, .Rolls = {{6, 2}},
        .Changes = {{"First.hitpoints.current", 98}, {"Actor.hands.count", 4}, {"Actor.actions.count", 2}}});
}

TEST_F(TGeneratedFlowTest, ThousandsOfImmediateChoicesDoNotGrowTheCallbackStack)
{
    constexpr int kIterations = 4096;
    Actor(kIterations);
    Target();
    for (int index = 0; index < kIterations; ++index) {
        io_.ExpectValue(0, EFlowDecision::Continue);
    }
    EXPECT_EQ(Execute("ImmediateFlow"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(0).Hands().Count(), kIterations);
    EXPECT_LE(io_.MaxDepth(), 2);
}

TEST_F(TGeneratedFlowTest, EmptyLoopSkipsChoicesAndRunsTheFollowingStatement)
{
    Actor(0);
    Target();
    Act("ImmediateFlow", 0, {.Changes = {{"Actor.hands.count", 0}, {"Actor.actions.count", 2}}});
}

TEST_F(TGeneratedFlowTest, AuthoredNamesDoNotCollideWithFrameSlotsOrContinuationInternals)
{
    Actor();
    Target();
    Check(Option("NameCollisionFlow"), {.Choose = {kContinue},
        .Changes = {{"Actor.hands.count", 24}, {"Actor.actions.count", 2}}});
    Check(Option("TypeCollisionFlow"), {.Choose = {kContinue},
        .Changes = {{"Actor.hands.count", 21}, {"Actor.actions.count", 2}}});
}

TEST_F(TGeneratedFlowTest, PositionChoicesMoveTheActorThroughTheJournal)
{
    Actor();
    Target();
    Check(Option("TraverseFlow"), {.Choose = {TPick::Value(TPosition{1, 0}), TPick::Value(TPosition{2, 1})},
        .Changes = {{"Actor.position", "TPosition{2,1}"}, {"Actor.hands.count", 3}, {"Actor.actions.count", 2}}});
    Check(Option("TraverseFlow"), {.Choose = {TPick::Value(TPosition{1, 0}), TPick::Value(TPosition{0, 1})},
        .Changes = {{"Actor.position", "TPosition{0,1}"}, {"Actor.hands.count", 1}, {"Actor.actions.count", 2}}});
}

TEST_F(TGeneratedFlowTest, VariantReplacementErasureAndResourcesShareTheJournal)
{
    Actor();
    Target();
    const auto mutated = kContinue.WhenOffered([this](const TAlternatives&) {
        EXPECT_EQ(Read(0).ConditionPenalty(), 5);
        EXPECT_FALSE(Read(0).Conditions().Has(EConditionKind::Prone));
        EXPECT_EQ(Read(0).Actions().Count(), 1);
        EXPECT_EQ(Read(0).Hands().Count(), 7);
    });
    Act("MutationFlow", 0, {.Choose = {mutated}, .Changes = {{"Actor.actions.count", 2}, {"Actor.hands.count", 7}}});
}

TEST_F(TGeneratedFlowTest, FailedResourceStatementsLeaveEarlierStatementsAndNoPartialUpdate)
{
    Add("Spender", {0, 0}, 0, 3, true);
    Add("Negative", {1, 0}, 0, -1, true);
    Add("Setter", {2, 0}, 0, 1, true);
    EXPECT_THROW(Execute("InvalidSpendFlow", 0), std::invalid_argument);
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    EXPECT_EQ(Read(0).Hands().Count(), 2);
    EXPECT_EQ(Read(0).ConditionPenalty(), 4);
    EXPECT_THROW(Execute("InvalidSpendFlow", 1), std::invalid_argument);
    EXPECT_EQ(Read(1).Hands().Count(), 2);
    EXPECT_THROW(Execute("NegativeSetFlow", 2), std::invalid_argument);
    EXPECT_EQ(Read(2).Actions().Count(), 2);
    EXPECT_THROW(Execute("OverflowFlow", 2), std::overflow_error);
    EXPECT_EQ(Read(2).Actions().Count(), std::numeric_limits<int>::max());
}

}
