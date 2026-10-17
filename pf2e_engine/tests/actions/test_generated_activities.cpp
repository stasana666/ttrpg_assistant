#include <pf2e_engine/inventory/generated_activities.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>

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
        TIdCollection<TWeapon> weapons;
        if (armed) {
            weapons.Add(CreateActivityBlade());
        }
        const int id = static_cast<int>(creatures_.size());
        return Join(TCreature::Create({
            .Level = level,
            .Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 100}), .Current = 100}),
            .Weapons = std::move(weapons),
            .Features = trained ? Features({"generated_activities.ActivityTraining"}) : Features({}),
            .Actions = TResource(3),
            .Hands = TResource(2),
        }), id, {id, 0}, std::move(name));
    }

    TPlayer& Actor(int level = 1, bool armed = false) { return Add("Actor", true, level, armed); }
    TPlayer& Target() { return Add("Target", false); }

    static std::map<std::string, TField> Spent(int hands, int target = 100)
    {
        std::map<std::string, TField> result = {{"Actor.actions.count", 2}, {"Actor.hands.count", hands}};
        if (target != 100) {
            result.emplace("Target.hitpoints.current", target);
        }
        return result;
    }
};

TEST_F(TGeneratedActivitiesTest, NestedTypedReturnsResumeEachParentOnce)
{
    Actor();
    Target();
    const auto option = Option("NestedActivities");
    const auto accepted = TPick::Value(EActivityDecision::Accepted);
    Check(option, {.Choose = {"Target", TPick::Value(2), accepted, TPick::Value(1), TPick::Value(2)},
                   .Changes = Spent(7, 93)});
    Check(option, {.Choose = {"Target", TPick::Value(2), accepted, TPick::Value(1), TPick::Value(4)},
                   .Changes = Spent(13, 93)});
    Check(option, {.Choose = {"Target", TPick::Value(2), TPick::Value(EActivityDecision::Declined), TPick::Value(2)},
                   .Changes = Spent(6)});
    Act(option, {.Choose = {"Target", TPick::Value(5), accepted, TPick::Value(3), TPick::Value(4)},
                 .Changes = Spent(25, 85)});
}

TEST_F(TGeneratedActivitiesTest, ImmediateNestedCompletionsDoNotNestCallbacks)
{
    Actor();
    Target();
    io_.ExpectName(0, {}, "Target");
    for (const int value : {5, 3, 4}) {
        io_.ExpectValue(0, value);
        if (value == 5) {
            io_.ExpectValue(0, EActivityDecision::Accepted);
        }
    }
    EXPECT_EQ(Execute("NestedActivities"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(io_.MaxDepth(), 1);
}

TEST_F(TGeneratedActivitiesTest, ThousandsOfImmediateCallsKeepTheLoopStackBounded)
{
    Actor(8192);
    EXPECT_EQ(Execute("ImmediateActivities"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(0).Hands().Count(), 8192);
    EXPECT_EQ(io_.Requests(), 0u);
}

TEST_F(TGeneratedActivitiesTest, DiscardedTypedResultsContinueOnce)
{
    Actor();
    Act("DiscardActivityResult", 0, {.Choose = {TPick::Value(2)}, .Changes = Spent(3)});
    Act("DiscardActivityResult", 0, {.Choose = {TPick::Value(5)},
                                     .Changes = {{"Actor.actions.count", 1}, {"Actor.hands.count", 4}}});
}

TEST_F(TGeneratedActivitiesTest, SourceSnapshotSurvivesNestedSuspensionAndTheNextCallReadsFreshValues)
{
    Actor(1, true);
    Target();
    io_.ExpectName(0, {}, "Target");
    io_.ExpectValue(0, 0, true);
    const auto point = Pause([&] { Execute("WeaponActivities"); });
    TTransformator editor(io_);
    auto execution = editor.ExecutionContext();
    auto weapon = execution.Project(Creature(0).Weapons(), [](TIdCollection<TWeapon>& values) {
        return TGuarded<TWeapon>(*values.Find(TId<TWeapon>{0}));
    });
    execution.AssignGuarded(execution.Project(weapon, [](TWeapon& value) { return value.BaseDiceSize(); }), 12, weapon,
                            "base_dice_size");
    point.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 96);
    EXPECT_EQ(Read(0).Hands().Count(), 5);
    Act("WeaponActivities", 0, {.Choose = {"Target", TPick::Value(1)},
        .Changes = {{"Actor.actions.count", 1}, {"Actor.hands.count", 14}, {"Target.hitpoints.current", 83}}});
}

TEST_F(TGeneratedActivitiesTest, ChildFailureKeepsPaidAndCompletedEffectsUntilTheCallerUndoes)
{
    Actor();
    Target();
    io_.ExpectName(0, {}, "Target");
    io_.ExpectValue(0, 1, true);
    const auto point = Pause([&] { Execute("FailingActivity"); });
    EXPECT_EQ(Read(1).Hitpoints().Current(), 97);
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    const auto paused = Snapshot();
    EXPECT_THROW(point.Resume(), std::runtime_error);
    ExpectSame(paused, Snapshot(), "failed child");
}

TEST_F(TGeneratedActivitiesTest, ActivityEntryRejectsForeignAndReadonlyEntityArguments)
{
    Actor();
    auto& target = Target();
    const auto context = Context();
    auto foreign_creature = TCreature::Create({});
    TPlayer foreign(&foreign_creature, TPlayerTeam{0}, TPlayerId{9}, "Foreign", "");
    int resumed = 0;
    std::function<void(int)> on_resume = [&](int) { ++resumed; };
    EXPECT_THROW(RunGeneratedActivitiesEntryProbe(context, BindActionEntity<TCreature>(foreign), on_resume),
                 TAbilityUnavailable);
    EXPECT_THROW(RunGeneratedActivitiesEntryProbe(context, BindActionEntity<TCreature>(std::as_const(target)), on_resume),
                 std::logic_error);
    EXPECT_THROW(RunGeneratedActivitiesEntryProbe(context, BindActionEntity<TCreature>(target), {}),
                 std::invalid_argument);
    EXPECT_EQ(RunGeneratedActivitiesEntryProbe(context, BindActionEntity<TCreature>(target), on_resume), 7);
    EXPECT_EQ(Read(0).Hands().Count(), 3);
    context->battle = nullptr;
    EXPECT_EQ(RunGeneratedActivitiesIncrement(context, 9, on_resume), 10);
    EXPECT_EQ(resumed, 0);
}

TEST_F(TGeneratedActivitiesTest, ActorAnchorRejectsAReplacedContextAndAcceptsTheOriginal)
{
    auto& actor = Actor();
    auto& other = Target();
    const auto context = Context();
    const auto self = ActionSelf<TCreature>(context);
    EXPECT_NO_THROW(ValidateActionActorBinding(context, self.ReadOnly().View()));
    EXPECT_THROW(ValidateActionActorBinding(context, BindActionEntity<TCreature>(other).View()), TAbilityUnavailable);
    EXPECT_THROW(ValidateActionActorBinding(context, TGeneratedEntityView{}), TAbilityUnavailable);
    auto foreign_creature = TCreature::Create({});
    TPlayer foreign(&foreign_creature, TPlayerTeam{0}, TPlayerId{0}, "Foreign", "");
    EXPECT_THROW(ValidateActionActorBinding(context, BindActionEntity<TCreature>(foreign).View()), TAbilityUnavailable);
    io_.ExpectValue(0, 2, true);
    const auto point = Pause([&] { Execute("DiscardActivityResult", actor.GetId()); });
    auto* const original = last_context_->actor;
    last_context_->actor = &other;
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    last_context_->actor = original;
    point.Resume();
    EXPECT_EQ(Read(0).Hands().Count(), 3);
    EXPECT_EQ(Read(1).Hands().Count(), 2);
}

TEST_F(TGeneratedActivitiesTest, ResumedActivityRevalidatesUnusedEntityArguments)
{
    Actor();
    auto& target = Target();
    const auto context = Context();
    int resumed = 0;
    std::function<void(int)> on_resume = [&](int value) {
        EXPECT_EQ(value, 7);
        ++resumed;
    };
    io_.ExpectValue(0, 7, true);
    const auto point = Pause([&] {
        RunGeneratedActivitiesPausedEntryProbe(context, BindActionEntity<TCreature>(target), on_resume);
    });
    context->battle = nullptr;
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    context->battle = battle_.get();
    auto moved = std::move(*target.GetCreature());
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(resumed, 0);
    *target.GetCreature() = std::move(moved);
    point.Resume();
    EXPECT_EQ(Read(0).Hands().Count(), 3);
    EXPECT_EQ(resumed, 1);
}

TEST_F(TGeneratedActivitiesTest, PositionResultKeepsTheCapturedOrigin)
{
    auto& actor = Actor();
    Act("PositionActivity", 0, {.Choose = {TPick::Value(TPosition{0, 1})},
        .Changes = {{"Actor.position", "TPosition{0,1}"}, {"Actor.actions.count", 2}, {"Actor.hands.count", 1}}});
    io_.ExpectValue(0, TPosition{0, 2}, true);
    const auto point = Pause([&] { Execute("PositionActivity"); });
    journal_->MovePlayer(&actor, {1, 1});
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
}

}
