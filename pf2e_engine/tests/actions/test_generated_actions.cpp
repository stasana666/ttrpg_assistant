#include <pf2e_engine/rules.h>
#include <pf2e_engine/inventory/generated_actions.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct TMember {
    int Hitpoints = 20;
    std::string Feature = {};
    int Temporary = 0;
    int Hands = 0;
    bool ClassFeature = false;
    int Level = 1;
    std::string ClassExtra = {};
};

TIdCollection<TFeatureSelection> ActionFeatures(const std::string& name)
{
    return name.empty() ? Features({}) : Features({"generated_actions." + name});
}

class TGeneratedActionsTest : public TBattleFixture {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedActionsRules(*rules);
        RegisterGeneratedActionsRules(*rules);
        StartBattle(std::move(rules), 8, "generated_actions.");
    }

    TPlayer& Add(std::string name, const TMember& member = {})
    {
        const int id = static_cast<int>(creatures_.size());
        return Join(TCreature::Create({
            .Level = member.Level,
            .Class = TClass::Create({.Features = ActionFeatures(member.ClassFeature ? member.Feature : member.ClassExtra)}),
            .Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 20}), .Current = member.Hitpoints,
                                             .Temporary = member.Temporary}),
            .Features = ActionFeatures(member.ClassFeature ? "" : member.Feature),
            .Actions = TResource(3),
            .Hands = TResource(member.Hands),
        }), id == 0 ? 0 : 1, {id, 0}, std::move(name));
    }

    void RemoveClassFeatures(TTransformator& journal)
    {
        journal.ExecutionContext().AssignGuarded(Creature(0).Class(), TClass::Create({}), TGuarded<TCreature>(Creature(0)),
                                                 "class");
    }
};

TEST_F(TGeneratedActionsTest, ClassFeatureGrantsAnActionThatTargetsAChosenLivingCreature)
{
    Add("Actor", {.Feature = "Mending", .ClassFeature = true});
    Add("First", {.Hitpoints = 6});
    Add("Second", {.Hitpoints = 18});
    Add("Defeated", {.Hitpoints = 0});
    const auto option = Option("Mend");
    EXPECT_EQ(option.Source, TAbilitySourcePath{}.Field("class").Field("features").Entry(0));
    EXPECT_FALSE(option.Provenance.empty());
    EXPECT_TRUE(battle_->AvailableActions(Player(1)).empty());
    Act(option, {.Choose = {"Second"}, .Rolls = {{6, 5}},
        .Changes = {{"Second.hitpoints.current", 20}, {"Actor.actions.count", 2}}});
    EXPECT_EQ(io_.Candidates(), (std::vector<std::string>{"Actor", "First", "Second"}));
    Act(option, {.Choose = {"First"}, .Rolls = {{6, 4}},
        .Changes = {{"First.hitpoints.current", 10}, {"Actor.actions.count", 1}}});
}

TEST_F(TGeneratedActionsTest, EntityBindingEqualityUsesIdentityWithoutGrantingMutableAccess)
{
    auto& actor = Add("Actor");
    auto& other = Add("Other");
    const auto readonly = BindActionEntity<TCreature>(std::as_const(actor));
    const auto writable = BindActionEntity<TCreature>(actor);
    const TGeneratedEntityBinding<TCreature> battle_readonly(BindGeneratedActionEntity(std::as_const(actor), battle_.get()));
    const TGeneratedEntityBinding<TCreature> battle_writable(BindGeneratedActionEntity(actor, battle_.get()));
    EXPECT_EQ(readonly, writable);
    EXPECT_EQ(readonly, battle_readonly);
    EXPECT_EQ(readonly, battle_writable);
    EXPECT_EQ(readonly.View(), battle_writable.View());
    EXPECT_EQ(&readonly.Read(), actor.GetCreature());
    EXPECT_THROW(readonly.Guard(), std::logic_error);
    EXPECT_THROW(battle_readonly.Guard(), std::logic_error);
    EXPECT_NO_THROW(writable.Guard());
    EXPECT_NO_THROW(battle_writable.Guard());
    EXPECT_NE(readonly, BindActionEntity<TCreature>(other));
}

TEST_F(TGeneratedActionsTest, EntityChoiceRejectsAParticipantOfAnotherBattle)
{
    auto& actor = Add("Actor");
    auto foreign_creature = TCreature::Create({.Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 20})})});
    TBattle foreign_battle(TBattleMap(2, 2), &rng_, io_);
    rng_.ExpectCall(20, 10);
    foreign_battle.AddPlayer(TPlayer(&foreign_creature, TPlayerTeam{1}, TPlayerId{0}, "Foreign", ""), {0, 0});
    auto& foreign_actor = *foreign_battle.GetIfPlayers([](const TPlayer*) { return true; }).at(0);
    const TGeneratedEntityBinding<TCreature> candidate(BindGeneratedActionEntity(foreign_actor, &foreign_battle));
    bool called = false;
    EXPECT_THROW(SelectActionEntity<TCreature>(Context(actor.GetId()), {candidate}, {},
        [&called](TGeneratedEntityBinding<TCreature>) { called = true; }), TAbilityUnavailable);
    EXPECT_FALSE(called);
}

TEST_F(TGeneratedActionsTest, DamageUsesTemporaryHitpointsOfTheChosenTargetOnly)
{
    Add("Actor", {.Feature = "Injuring"});
    Add("First", {.Hitpoints = 10, .Temporary = 3});
    Add("Second", {.Hitpoints = 17, .Temporary = 5});
    Act("Harm", 0, {.Choose = {"First"},
        .Changes = {{"First.hitpoints.current", 6}, {"First.hitpoints.temporary", 0}, {"Actor.actions.count", 2}}});
}

TEST_F(TGeneratedActionsTest, CostCanUseAnyDeclaredResourceAndHidesTheActionOnceSpent)
{
    Add("Actor", {.Feature = "HeldAidTraining", .Hands = 2});
    Add("Target", {.Hitpoints = 18, .Temporary = 2});
    Add("Other", {.Feature = "HeldAidTraining", .Hands = 1});
    const auto option = Option("HeldAid");
    EXPECT_FALSE(Available("HeldAid", 2));
    Act(option, {.Choose = {"Target"}, .Changes = {{"Target.hitpoints.current", 20}, {"Target.hitpoints.temporary", 4},
                                                   {"Actor.hands.count", 0}}});
    EXPECT_FALSE(Available("HeldAid"));
    Act(option, {.Result = EAbilityExecutionResult::Unavailable});
}

TEST_F(TGeneratedActionsTest, SuccessiveChoicesKeepRolledLocalsAcrossSuspension)
{
    Add("Actor", {.Feature = "RelayTraining"});
    Add("First", {.Hitpoints = 5});
    Add("Second", {.Hitpoints = 15});
    Act("Relay", 0, {.Choose = {"First", "Second"}, .Rolls = {{6, 4}},
        .Changes = {{"First.hitpoints.current", 9}, {"Second.hitpoints.current", 11}, {"Actor.actions.count", 2}}});
}

TEST_F(TGeneratedActionsTest, FailedRequireKeepsCompletedStepsUntilTheCallerRollsBack)
{
    Add("Actor", {.Feature = "RestrictedAidTraining"});
    Add("Target", {.Hitpoints = 5});
    Add("Higher", {.Hitpoints = 12, .Level = 2});
    const auto option = Option("RestrictedAid");
    const auto start = journal_->CurrentState();
    io_.ExpectName(0, {}, "Target");
    EXPECT_THROW(Execute(option), std::runtime_error);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 7);
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    journal_->Undo(start);
    Act(option, {.Choose = {"Higher"}, .Changes = {{"Higher.hitpoints.current", 5}, {"Actor.actions.count", 2}}});
}

TEST_F(TGeneratedActionsTest, ResumeRechecksTheTargetTheGrantAndTheActorBeforeRolling)
{
    Add("Actor", {.Feature = "Mending", .ClassFeature = true});
    Add("Target", {.Hitpoints = 5});
    io_.ExpectName(0, {}, "Target", true);
    const auto point = Pause([&]() { Execute(Option("Mend")); });
    const auto chosen = journal_->CurrentState();
    for (const auto& invalidate : std::vector<std::function<void()>>{
             [&] { journal_->DealDamage(&Player(1), 5); },
             [&] { RemoveClassFeatures(*journal_); },
             [&] { journal_->DealDamage(&Player(0), 20); }}) {
        invalidate();
        EXPECT_THROW(point.Resume(), TAbilityUnavailable);
        journal_->Undo(chosen);
    }
    rng_.ExpectCall(6, 3);
    point.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 8);
    EXPECT_EQ(Read(0).Actions().Count(), 2);
}

TEST_F(TGeneratedActionsTest, ADefeatedActorCannotStartAnAction)
{
    Add("Actor", {.Feature = "Mending"});
    Add("Target", {.Hitpoints = 5});
    const auto option = Option("Mend");
    journal_->DealDamage(&Player(0), 20);
    EXPECT_FALSE(Available("Mend"));
    Act(option, {.Result = EAbilityExecutionResult::Unavailable});
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Unavailable);
}

TEST_F(TGeneratedActionsTest, FeatureEventsCollectContributionsOfTheOwnersSelectedFeatures)
{
    Add("Actor", {.Feature = "AssistedMending"});
    Add("Target", {.Hitpoints = 2, .Feature = "Helpful"});
    Add("Helped", {.Feature = "HelpfulMending"});
    Add("Diamond", {.Feature = "DiamondMending", .ClassExtra = "Helpful"});
    Add("Leveled", {.Feature = "ParameterizedMending", .Level = 2});
    Add("Dice", {.Feature = "DiceMending"});
    EXPECT_FALSE(Available("AssistedMend", 1));
    const auto helped = CollectAbilities(Read(2), battle_->AbilityRules().Catalog);
    ASSERT_EQ(helped.Grants().size(), 1u);
    EXPECT_TRUE(std::ranges::any_of(helped.Features(), [](const TGrantedAbility& feature) {
        return feature.DefinitionId == "generated_actions.Helpful";
    }));
    const auto diamond = CollectAbilities(Read(3), battle_->AbilityRules().Catalog);
    EXPECT_EQ(std::ranges::count_if(diamond.Features(), [](const TGrantedAbility& feature) {
        return feature.DefinitionId == "generated_actions.Helpful";
    }), 2);
    struct TCase {
        int Actor;
        std::string Action;
        std::vector<std::pair<int, int>> Rolls;
        int Hitpoints;
    };
    for (const auto& test : std::vector<TCase>{{0, "AssistedMend", {{6, 3}}, 5}, {2, "AssistedMend", {{6, 3}}, 7},
                                              {3, "AssistedMend", {{6, 3}}, 9}, {4, "AssistedMend", {{6, 4}}, 15},
                                              {5, "DiceMend", {{8, 3}, {4, 1}, {4, 4}}, 10}}) {
        SCOPED_TRACE(test.Actor);
        const auto actor = std::string(Player(test.Actor).GetName());
        Check(Option(test.Action, test.Actor), {.Choose = {"Target"}, .Rolls = test.Rolls,
            .Changes = {{"Target.hitpoints.current", test.Hitpoints}, {actor + ".actions.count", 2}}});
    }
}

TEST_F(TGeneratedActionsTest, EventsReadTheSourcesPresentWhenTheyAreDispatched)
{
    Add("Actor", {.Feature = "AssistedMending", .ClassExtra = "Helpful"});
    Add("Target", {.Hitpoints = 5});
    io_.ExpectName(0, {}, "Target", true);
    const auto point = Pause([&]() { Execute(Option("AssistedMend")); });
    RemoveClassFeatures(*journal_);
    rng_.ExpectCall(6, 3);
    point.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 8);
}

TEST_F(TGeneratedActionsTest, FoldedEventValuesSurviveLaterSourceChanges)
{
    Add("Actor", {.Feature = "EventRelayTraining", .ClassExtra = "Helpful"});
    Add("First", {.Hitpoints = 5});
    Add("Second", {.Hitpoints = 3});
    io_.ExpectName(0, {}, "First");
    io_.ExpectName(0, {}, "Second", true);
    rng_.ExpectCall(6, 4);
    const auto point = Pause([&]() { Execute(Option("EventRelay")); });
    TTransformator editor(io_);
    RemoveClassFeatures(editor);
    point.Resume();
    EXPECT_EQ(Read(2).Hitpoints().Current(), 9);
}

}
