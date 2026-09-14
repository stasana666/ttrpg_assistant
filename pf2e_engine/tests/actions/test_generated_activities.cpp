#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/inventory/generated_activities.h>

#include "../test_lib/mock_dice_roller.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <any>
#include <deque>
#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

class TActivityChoiceIo final : public IInteractionSystem {
public:
    template <class T>
    void ExpectValue(T value, bool suspend = false)
    {
        expected_.push_back({[value](const TAlternative& alternative) {
            const auto* actual = std::any_cast<T>(&alternative.value);
            return actual != nullptr && *actual == value;
        }, suspend});
    }

    void ExpectTarget(bool suspend = false)
    {
        expected_.push_back({[](const TAlternative& alternative) {
            return alternative.name == "Target";
        }, suspend});
    }

    void RequestChoice(int player_id, TAlternatives alternatives, TState state,
                       std::function<void(size_t)> receive) override
    {
        if (expected_.empty()) {
            throw std::logic_error("unexpected activity choice");
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
            throw std::logic_error("unexpected activity alternative");
        }
        auto expected = std::move(expected_.front());
        expected_.pop_front();
        for (size_t index = 0; index < alternatives.Size(); ++index) {
            if (expected.Matches(alternatives[index])) {
                return index;
            }
        }
        throw std::logic_error("expected activity alternative is absent");
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

class TGeneratedActivitiesTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedActivitiesRules(*rules);
        RegisterGeneratedActivitiesRules(*rules);
        nlohmann::json map = {{"x_size", 4}, {"y_size", 4}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, std::move(rules));
        journal_ = std::make_unique<TTransformator>(io_);
    }

    void TearDown() override
    {
        rng_.Verify();
        io_.Verify();
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
        creatures_.push_back(TCreature::Create({
            .Level = level,
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = 100}), .Current = 100,
            }),
            .Weapons = std::move(weapons),
            .Features = std::move(features),
            .Actions = TResource(3),
            .Hands = TResource(2),
        }));
        rng_.ExpectCall(20, 10);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{id}, TPlayerId{id},
                                   std::move(name), ""), {id, 0});
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    TPlayer& Actor(int level = 1, bool armed = false)
    {
        return Add("Actor", true, level, armed);
    }

    TPlayer& Target()
    {
        return Add("Target", false);
    }

    std::shared_ptr<TActionContext> Context(TPlayer* actor = nullptr)
    {
        auto context = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(), .dice_roller = &rng_, .transformator = journal_.get(),
            .io_system = &io_, .scheduler = nullptr, .effect_manager = nullptr,
        });
        if (actor != nullptr) {
            context->game_object_registry = std::make_shared<TGameObjectRegistry>();
            context->game_object_registry->Add(
                TGameObjectIdManager::Instance().Register("self"), actor);
        }
        return context;
    }

    EAbilityExecutionResult Execute(TPlayer& actor, const std::string& name)
    {
        const auto options = battle_->AvailableActions(actor);
        const auto found = std::ranges::find_if(options, [&](const TAbilityOption& option) {
            return option.DefinitionId == "generated_activities." + name;
        });
        if (found == options.end()) {
            throw std::logic_error("missing activity action: " + name);
        }
        const auto& rules = battle_->AbilityRules();
        last_context_ = Context();
        return ExecuteAbility(*found, last_context_, actor,
            CollectAbilities(*actor.GetCreature(), rules.Catalog), rules.Executions,
            {.ValidateSource = [this, &actor, definition = found->DefinitionId,
                                source = found->Source]() {
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
        throw std::logic_error("activity did not suspend");
    }

    TAstNode Snapshot() const
    {
        TAstContext context;
        auto node = TAstNode::MakeObject("generated_activity_execution");
        AddOwnedObject(node, "journal", *journal_, context);
        AddOwnedObject(node, "battle", *battle_, context);
        return node;
    }

    TMockRng rng_;
    TActivityChoiceIo io_;
    std::deque<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> journal_;
    std::shared_ptr<TActionContext> last_context_;
};

TEST_F(TGeneratedActivitiesTest, NestedTypedReturnsResumeEachParentOnceAndReplayIndependentFrames)
{
    auto& actor = Actor();
    auto& target = Target();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectTarget();
    io_.ExpectValue(2, true);
    const auto amount_point = Suspend([&] { Execute(actor, "NestedActivities"); });
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100);

    io_.ExpectValue(EActivityDecision::Accepted, true);
    const auto outcome_point = Suspend([&] { amount_point.Resume(); });
    io_.ExpectValue(1, true);
    const auto extra_point = Suspend([&] { outcome_point.Resume(); });
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 97);
    io_.ExpectValue(2, true);
    const auto parent_point = Suspend([&] { extra_point.Resume(); });
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
    io_.ExpectValue(4);
    extra_point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 93);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 13);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);

    amount_point.Revert(*journal_);
    io_.ExpectValue(EActivityDecision::Declined);
    io_.ExpectValue(2);
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
    io_.ExpectTarget();
    io_.ExpectValue(5);
    io_.ExpectValue(EActivityDecision::Accepted);
    io_.ExpectValue(3);
    io_.ExpectValue(4);
    EXPECT_EQ(Execute(actor, "NestedActivities"), EAbilityExecutionResult::Applied);
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
    EXPECT_EQ(Execute(actor, "ImmediateActivities"), EAbilityExecutionResult::Applied);
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
    io_.ExpectValue(2);
    EXPECT_EQ(Execute(actor, "DiscardActivityResult"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    io_.ExpectValue(5, true);
    const auto point = Suspend([&] { Execute(actor, "DiscardActivityResult"); });
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
    io_.ExpectTarget();
    io_.ExpectValue(0, true);
    const auto point = Suspend([&] { Execute(actor, "WeaponActivities"); });

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
    io_.ExpectTarget();
    io_.ExpectValue(1);
    EXPECT_EQ(Execute(actor, "WeaponActivities"), EAbilityExecutionResult::Applied);
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
    io_.ExpectTarget();
    io_.ExpectValue(0, true);
    const auto point = Suspend([&] { Execute(actor, "WeaponActivities"); });
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
    io_.ExpectTarget();
    io_.ExpectValue(1, true);
    const auto point = Suspend([&] { Execute(actor, "FailingActivity"); });
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
    const auto context = Context(&actor);
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
    const auto context = Context(&actor);
    const auto self = ActionSelf<TCreature>(context);
    EXPECT_NO_THROW(ValidateActionActorBinding(context, self.ReadOnly().View()));
    EXPECT_THROW(ValidateActionActorBinding(context, BindActionEntity<TCreature>(other).View()),
                 TAbilityUnavailable);
    EXPECT_THROW(ValidateActionActorBinding(context, TGeneratedEntityView{}), TAbilityUnavailable);
    auto foreign_creature = TCreature::Create({});
    TPlayer foreign(&foreign_creature, TPlayerTeam{0}, TPlayerId{0}, "Foreign", "");
    EXPECT_THROW(ValidateActionActorBinding(context, BindActionEntity<TCreature>(foreign).View()),
                 TAbilityUnavailable);

    io_.ExpectValue(2, true);
    const auto point = Suspend([&] { Execute(actor, "DiscardActivityResult"); });
    const auto registry = last_context_->game_object_registry;
    last_context_->game_object_registry = Context(&other)->game_object_registry;
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
    const auto context = Context(&actor);
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    int resumed = 0;
    std::function<void(int)> on_resume = [&](int value) {
        EXPECT_EQ(value, 7);
        ++resumed;
    };
    io_.ExpectValue(7, true);
    const auto point = Suspend([&] {
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
    io_.ExpectValue(TPosition{0, 1}, true);
    const auto point = Suspend([&] { Execute(actor, "PositionActivity"); });
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
