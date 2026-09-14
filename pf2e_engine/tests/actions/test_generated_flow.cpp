#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/inventory/generated_flow.h>

#include "../test_lib/mock_dice_roller.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <any>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

class TFlowChoiceIo final : public IInteractionSystem {
public:
    template <class T>
    void ExpectValue(T value, bool suspend = false)
    {
        expected_.push_back({[value](const TAlternative& alternative) {
            const auto* actual = std::any_cast<T>(&alternative.value);
            return actual != nullptr && *actual == value;
        }, suspend});
    }

    void ExpectTarget(std::string name, bool suspend = false)
    {
        expected_.push_back({[name = std::move(name)](const TAlternative& alternative) {
            return alternative.name == name;
        }, suspend});
    }

    void RequestChoice(int player_id, TAlternatives alternatives, TState state,
                       std::function<void(size_t)> receive) override
    {
        if (expected_.empty()) {
            throw std::logic_error("unexpected flow choice");
        }
        const bool suspend = expected_.front().Suspend;
        const auto index = ChooseAlternativeIndex(player_id, alternatives);
        ++requests_;
        if (suspend) {
            throw TSavepointStackUnwind(state,
                [receive = std::move(receive), index]() { receive(index); });
        }
        ++depth_;
        max_depth_ = std::max(max_depth_, depth_);
        try {
            receive(index);
        } catch (...) {
            --depth_;
            throw;
        }
        --depth_;
    }

    void HandleReactionTrigger(const TTriggerContext&, const TState&) override {}
    std::ostream& GameLog() override { return log_; }
    std::ostream& DevLog() override { return log_; }
    void Verify() const { EXPECT_TRUE(expected_.empty()); }
    size_t Requests() const { return requests_; }
    int MaxDepth() const { return max_depth_; }

protected:
    size_t ChooseAlternativeIndex(int player_id, const TAlternatives& alternatives) override
    {
        EXPECT_EQ(player_id, 0);
        if (expected_.empty()) {
            throw std::logic_error("unexpected flow alternative");
        }
        auto expected = std::move(expected_.front());
        expected_.pop_front();
        for (size_t index = 0; index < alternatives.Size(); ++index) {
            if (expected.Matches(alternatives[index])) {
                return index;
            }
        }
        throw std::logic_error("expected flow alternative is absent");
    }

private:
    struct TExpected {
        std::function<bool(const TAlternative&)> Matches;
        bool Suspend;
    };

    std::deque<TExpected> expected_;
    size_t requests_ = 0;
    int depth_ = 0;
    int max_depth_ = 0;
    std::ostringstream log_;
};

class TGeneratedFlowTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedFlowRules(*rules);
        RegisterGeneratedFlowRules(*rules);
        nlohmann::json map = {{"x_size", 8}, {"y_size", 8}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, std::move(rules));
        journal_ = std::make_unique<TTransformator>(io_);
    }

    void TearDown() override
    {
        rng_.Verify();
        io_.Verify();
    }

    TPlayer& Add(std::string name, TPosition position, int team, int level, bool trained)
    {
        TIdCollection<TFeatureSelection> features;
        if (trained) {
            features.Add(TFeatureSelection("generated_flow.FlowTraining"));
        }
        const int id = static_cast<int>(creatures_.size());
        creatures_.push_back(TCreature::Create({
            .Level = level,
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = 100}),
                .Current = 100,
            }),
            .Features = features,
            .Actions = TResource(3),
            .Hands = TResource(2),
        }));
        rng_.ExpectCall(20, 10);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{team}, TPlayerId{id},
                                   std::move(name), ""), position);
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    TPlayer& Actor(int level = 1)
    {
        return Add("Actor", {0, 0}, 0, level, true);
    }

    TPlayer& Target(std::string name = "Target", int level = 1, TPosition position = {7, 7})
    {
        return Add(std::move(name), position, 1, level, false);
    }

    TAbilityOption Option(TPlayer& actor, const std::string& name) const
    {
        const auto options = battle_->AvailableActions(actor);
        const auto found = std::ranges::find_if(options, [&](const TAbilityOption& option) {
            return option.DefinitionId == "generated_flow." + name;
        });
        if (found == options.end()) {
            throw std::logic_error("missing flow action: " + name);
        }
        return *found;
    }

    EAbilityExecutionResult Execute(TPlayer& actor, const std::string& name)
    {
        const auto option = Option(actor, name);
        const auto& rules = battle_->AbilityRules();
        auto context = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(),
            .dice_roller = &rng_,
            .transformator = journal_.get(),
            .io_system = &io_,
            .scheduler = nullptr,
            .effect_manager = nullptr,
        });
        return ExecuteAbility(option, std::move(context), actor,
            CollectAbilities(*actor.GetCreature(), rules.Catalog), rules.Executions,
            {.ValidateSource = [this, &actor, definition = option.DefinitionId,
                                source = option.Source]() {
                const auto fresh = CollectAbilities(*actor.GetCreature(),
                                                     battle_->AbilityRules().Catalog);
                return std::ranges::any_of(fresh.Grants(), [&](const TGrantedAbility& grant) {
                    return grant.DefinitionId == definition && grant.Source == source;
                });
            }});
    }

    static TSavepointStackUnwind Suspend(const std::function<void()>& work)
    {
        try {
            work();
        } catch (const TSavepointStackUnwind& point) {
            return point;
        }
        throw std::logic_error("flow action did not suspend");
    }

    TAstNode Snapshot() const
    {
        TAstContext context;
        auto node = TAstNode::MakeObject("generated_flow_execution");
        AddOwnedObject(node, "journal", *journal_, context);
        AddOwnedObject(node, "battle", *battle_, context);
        return node;
    }

    TMockRng rng_;
    TFlowChoiceIo io_;
    std::deque<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> journal_;
};

TEST_F(TGeneratedFlowTest, NestedBranchChoicesPreserveOuterLocalsAndReplayWithoutReroll)
{
    auto& actor = Actor();
    auto& target = Target("Target", 2);
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectValue(EFlowDecision::Continue);
    rng_.ExpectCall(6, 4);
    io_.ExpectTarget("Target", true);
    auto target_point = Suspend([&]() { Execute(actor, "BranchFlow"); });
    EXPECT_EQ(rng_.RemainingCalls(), 0u);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100);

    io_.ExpectValue(EFlowDecision::Continue, true);
    auto boost_point = Suspend([&]() { target_point.Resume(); });
    const auto paused = Snapshot();
    boost_point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 93);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 9);
    boost_point.Revert(*journal_);
    EXPECT_EQ(Snapshot(), paused);
    boost_point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 93);

    target_point.Revert(*journal_);
    io_.ExpectValue(EFlowDecision::Finish);
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
    io_.ExpectValue(EFlowDecision::Finish, true);
    auto point = Suspend([&]() { Execute(actor, "BranchFlow"); });
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
    io_.ExpectValue(EFlowDecision::Continue);
    io_.ExpectTarget("First", true);
    auto first_point = Suspend([&]() { Execute(actor, "LoopFlow"); });
    io_.ExpectValue(EFlowDecision::Continue);
    io_.ExpectTarget("Second", true);
    auto second_point = Suspend([&]() { first_point.Resume(); });
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
    io_.ExpectValue(EFlowDecision::Continue);
    io_.ExpectTarget("Second", true);
    auto replayed = Suspend([&]() { first_point.Resume(); });
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
    io_.ExpectValue(EFlowDecision::Continue);
    io_.ExpectTarget("Target");
    io_.ExpectValue(EFlowDecision::Finish, true);
    auto point = Suspend([&]() { Execute(actor, "LoopFlow"); });
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
        io_.ExpectValue(EFlowDecision::Continue);
    }
    EXPECT_EQ(Execute(actor, "ImmediateFlow"), EAbilityExecutionResult::Applied);
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
    EXPECT_EQ(Execute(actor, "ImmediateFlow"), EAbilityExecutionResult::Applied);
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
    io_.ExpectValue(EFlowDecision::Continue, true);
    auto point = Suspend([&]() { Execute(actor, "NameCollisionFlow"); });
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
    io_.ExpectValue(EFlowDecision::Continue, true);
    auto point = Suspend([&]() { Execute(actor, "TypeCollisionFlow"); });
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
    io_.ExpectValue(TPosition{1, 0}, true);
    auto first_point = Suspend([&]() { Execute(actor, "TraverseFlow"); });
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

    io_.ExpectValue(TPosition{2, 1}, true);
    auto second_point = Suspend([&]() { first_point.Resume(); });
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
    io_.ExpectValue(TPosition{0, 1});
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
    io_.ExpectValue(EFlowDecision::Continue, true);
    auto point = Suspend([&]() { Execute(actor, "MutationFlow"); });
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
    EXPECT_THROW(Execute(actor, "InvalidSpendFlow"), std::invalid_argument);
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
    EXPECT_THROW(Execute(actor, "InvalidSpendFlow"), std::invalid_argument);
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
    EXPECT_THROW(Execute(actor, "NegativeSetFlow"), std::invalid_argument);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);

    EXPECT_THROW(Execute(actor, "OverflowFlow"), std::overflow_error);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), std::numeric_limits<int>::max());
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

}
