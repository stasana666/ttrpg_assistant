#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/reaction_window.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/inventory/generated_reactions.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

class TGeneratedReactionsTest : public TBattleFixture {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedReactionsRules(*rules);
        StartBattle(std::move(rules), 8, "generated_reactions.");
    }

    TPlayer& Add(std::string feature, bool armed = false, bool bound = false, int reach = 1)
    {
        TIdCollection<TFeatureSelection> features;
        if (!feature.empty()) {
            features.Add(TFeatureSelection("generated_reactions." + feature));
        }
        TIdCollection<TWeapon> weapons;
        if (armed) {
            TIdCollection<TFeatureSelection> weapon_features;
            if (bound) {
                weapon_features.Add(TFeatureSelection("generated_reactions.BoundTraining"));
            }
            weapons.Add(TWeapon::Create({.Name = "Blade", .BaseDiceSize = 4,
                .Reach = reach, .Features = std::move(weapon_features)}));
        }
        const int id = static_cast<int>(creatures_.size());
        return Join(TCreature::Create({
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = 100}), .Current = 100,
            }),
            .Weapons = std::move(weapons), .Features = std::move(features),
            .Actions = TResource(3), .Reactions = TResource(4), .Hands = TResource(0),
        }), id, {0, id}, "Actor " + std::to_string(id));
    }

    void RemoveFeatures(TTransformator& editor, int id)
    {
        editor.ExecutionContext().AssignGuarded(Creature(id).Features(),
            TIdCollection<TFeatureSelection>{}, TGuarded<TCreature>(Creature(id)), "features");
    }

    void RemoveWeapon(TTransformator& editor)
    {
        auto weapons = Read(1).Weapons();
        weapons.Remove(TId<TWeapon>{0});
        editor.ExecutionContext().AssignGuarded(Creature(1).Weapons(), std::move(weapons),
            TGuarded<TCreature>(Creature(1)), "weapons");
    }

    void SetWeapon(TTransformator& editor, int value, bool reach = false)
    {
        auto execution = editor.ExecutionContext();
        auto weapon = execution.Project(Creature(1).Weapons(), [](TIdCollection<TWeapon>& weapons) {
            return TGuarded<TWeapon>(*weapons.Find(TId<TWeapon>{0}));
        });
        auto field = execution.Project(weapon, [reach](TWeapon& item) {
            return reach ? item.Reach() : item.BaseDiceSize();
        });
        execution.AssignGuarded(field, value, weapon, reach ? "reach" : "base_dice_size");
    }
};

TEST_F(TGeneratedReactionsTest, RegistersTypedBodiesWithoutSyntheticActions)
{
    Add("ParentTraining");
    Add("ImmediateTraining");
    EXPECT_EQ(rules_->Executions.Find("generated_reactions.ImmediateCounter"), nullptr);
    EXPECT_NE(rules_->Reactions.Find<TTriggerFixtureChange>("generated_reactions.ImmediateCounter"), nullptr);
    EXPECT_TRUE(battle_->AvailableActions(Player(1)).empty());
}

TEST_F(TGeneratedReactionsTest, ImmediatePayloadAndDeclinePayOnlyTheAcceptedReaction)
{
    Add("ParentTraining");
    Add("ImmediateTraining");
    Add("ImmediateTraining");
    Act("ChangeWithWindow", 0, {.Choose = {TPick(1, "ImmediateCounter"), TPick(2, "Decline")},
        .Changes = {{"Actor 0.hands.count", 17}, {"Actor 0.actions.count", 2}, {"Actor 1.reactions.count", 3},
                    {"Actor 1.hands.count", 1}}});
}

TEST_F(TGeneratedReactionsTest, SuspendedReactionBodyAndParentTailReplayWithoutPayingTwice)
{
    Add("ParentTraining");
    Add("SlowTraining");
    Act("ChangeWithWindow", 0, {.Choose = {TPick(1, "SlowCounter"), TPick::Value(2, 1), TPick::Value(4, 1)},
        .Changes = {{"Actor 0.hands.count", 14}, {"Actor 0.actions.count", 2}, {"Actor 1.reactions.count", 3},
                    {"Actor 1.hands.count", 6}}});
}

TEST_F(TGeneratedReactionsTest, SourceRequirementsReadLiveStateBeforeReactionPayment)
{
    Add("ParentTraining");
    Add("", true, true);
    io_.ExpectName(1, "reaction", "BoundCounter", true);
    const auto offer = Pause([&] { Execute("ChangeWithWindow"); });
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    SetWeapon(editor, 0, true);
    EXPECT_THROW(offer.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Read(1).Reactions().Count(), 4);
    editor.Undo(edit);
    io_.ExpectValue(1, 0);
    offer.Resume();
    EXPECT_EQ(Read(0).Hands().Count(), 16);
    EXPECT_EQ(Read(1).Reactions().Count(), 3);
}

TEST_F(TGeneratedReactionsTest, MissingBoundSourceAndEmptyLeadingSourceChoiceOfferNothing)
{
    Add("ParentTraining");
    Add("BoundTraining");
    Add("SelectedTraining");
    Act("ChangeWithWindow", 0, {.Changes = {{"Actor 0.hands.count", 20}, {"Actor 0.actions.count", 2}}});
}

TEST_F(TGeneratedReactionsTest, BoundSourceValuesRemainSnapshotsAcrossSuspension)
{
    Add("ParentTraining");
    Add("", true, true);
    io_.ExpectName(1, "reaction", "BoundCounter");
    io_.ExpectValue(1, 0, true);
    const auto point = Pause([&] { Execute("ChangeWithWindow"); });
    TTransformator editor(io_);
    SetWeapon(editor, 8);
    point.Resume();
    EXPECT_EQ(Read(0).Hands().Count(), 16);
}

TEST_F(TGeneratedReactionsTest, SelectedWeaponNeedsNoGrant)
{
    Add("ParentTraining");
    Add("SelectedTraining", true);
    EXPECT_EQ(Read(1).Weapons().Find(TId<TWeapon>{0})->Features().Size(), 0U);
    Act("ChangeWithWindow", 0, {.Choose = {TPick(1, "SelectedCounter"), TPick(1, "Blade"), TPick::Value(1, 1)},
        .Changes = {{"Actor 0.hands.count", 15}, {"Actor 0.actions.count", 2}, {"Actor 1.reactions.count", 3},
                    {"Actor 1.hands.count", 1}}});
}

TEST_F(TGeneratedReactionsTest, ReactionFinishesAfterTheParentLosesItsGrantThenTheParentStops)
{
    Add("ParentTraining");
    Add("SlowTraining");
    io_.ExpectName(1, "reaction", "SlowCounter");
    io_.ExpectValue(1, 1, true);
    const auto point = Pause([&] { Execute("ChangeWithWindow"); });
    TTransformator editor(io_);
    RemoveFeatures(editor, 0);
    io_.ExpectValue(1, 3);
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Read(1).Hands().Count(), 4);
    EXPECT_EQ(Read(1).Reactions().Count(), 3);
    EXPECT_EQ(Read(0).Hands().Count(), 0);
}

TEST_F(TGeneratedReactionsTest, ReactionFinishesAfterTheParentDiesAndKeepsItsPayment)
{
    Add("ParentTraining");
    Add("KillingTraining");
    io_.ExpectName(1, "reaction", "KillingCounter");
    io_.ExpectValue(1, 2, true);
    const auto point = Pause([&] { Execute("ChangeWithWindow"); });
    EXPECT_TRUE(Read(0).IsDead());
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Read(1).Hands().Count(), 4);
    EXPECT_EQ(Read(1).Reactions().Count(), 3);
    EXPECT_EQ(Read(0).Actions().Count(), 0);
}

TEST_F(TGeneratedReactionsTest, OneActivitySharesTheLimitAcrossCellsAndReactionDefinitions)
{
    Add("ParentTraining");
    Add("LimitedTraining");
    Act("WalkOnce", 0, {.Choose = {TPick::Value(TPosition{1, 0}), TPick(1, "LimitedSecond"), TPick::Value(TPosition{2, 0}),
                                   TPick::Value(TPosition{3, 0})},
        .Changes = {{"Actor 0.position", "TPosition{3,0}"}, {"Actor 0.hands.count", 10}, {"Actor 0.actions.count", 2},
                    {"Actor 1.hands.count", 10}, {"Actor 1.reactions.count", 3}}});
}

TEST_F(TGeneratedReactionsTest, SubordinateActivityCallsGetFreshScopes)
{
    Add("ParentTraining");
    Add("LimitedTraining");
    Act("WalkTwice", 0, {.Choose = {TPick::Value(TPosition{1, 0}), TPick(1, "LimitedFirst"), TPick::Value(TPosition{2, 0}),
                                    TPick::Value(TPosition{3, 0}), TPick(1, "LimitedSecond"),
                                    TPick::Value(TPosition{4, 0})},
        .Changes = {{"Actor 0.position", "TPosition{4,0}"}, {"Actor 0.hands.count", 29}, {"Actor 0.actions.count", 2},
                    {"Actor 1.hands.count", 11}, {"Actor 1.reactions.count", 2}}});
}

TEST_F(TGeneratedReactionsTest, MultipleSubscriptionsHaveIndependentLimitKeys)
{
    Add("ParentTraining");
    Add("LimitedTraining");
    const auto first = rules_->Reactions.Find<TTriggerFixtureMove>("generated_reactions.LimitedFirst");
    const auto second = rules_->Reactions.Find<TTriggerFixtureOtherMove>("generated_reactions.LimitedFirst");
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first->LimitKey, second->LimitKey);
    Act("OtherWindow", 0, {.Choose = {TPick(1, "LimitedFirst"), TPick(1, "LimitedFirst")},
        .Changes = {{"Actor 0.hands.count", 18}, {"Actor 0.actions.count", 2}, {"Actor 1.hands.count", 2},
                    {"Actor 1.reactions.count", 2}}});
}

TEST_F(TGeneratedReactionsTest, NestedWindowOfAnotherTriggerReturnsThroughBothReactions)
{
    Add("ParentTraining");
    Add("NestedTraining");
    Add("InnerTraining");
    Act("ChangeWithWindow", 0, {.Choose = {TPick(1, "NestedCounter"), TPick(2, "InnerCounter"), TPick::Value(2, 2)},
        .Changes = {{"Actor 0.hands.count", 18}, {"Actor 0.actions.count", 2}, {"Actor 1.hands.count", 1},
                    {"Actor 1.reactions.count", 3}, {"Actor 2.hands.count", 2}, {"Actor 2.reactions.count", 3}}});
}

TEST_F(TGeneratedReactionsTest, NativeBodyEntryRejectsReadonlyForeignAndMissingReturnChannel)
{
    Add("ParentTraining");
    Add("SlowTraining");
    auto context = Context(1);
    const auto subject = BindActionEntity<TCreature>(Player(0));
    const auto callback = [](TReactionPayload) {};
    EXPECT_THROW(RunGeneratedReactionsSlowCounter(context, TReactionPayload::Create({}),
        TTriggerFixtureChange::TArguments{subject.ReadOnly(), 1}, callback), std::logic_error);
    auto foreign_creature = TCreature::Create({});
    TPlayer foreign(&foreign_creature, TPlayerTeam{0}, TPlayerId{0}, "Foreign", "");
    EXPECT_THROW(RunGeneratedReactionsSlowCounter(context, TReactionPayload::Create({}),
        TTriggerFixtureChange::TArguments{BindActionEntity<TCreature>(foreign), 1}, callback), TAbilityUnavailable);
    EXPECT_THROW(RunGeneratedReactionsSlowCounter(context, TReactionPayload::Create({}),
        TTriggerFixtureChange::TArguments{subject, 1}, {}), std::invalid_argument);
    EXPECT_EQ(io_.Requests(), 0U);
}

TEST_F(TGeneratedReactionsTest, NativeBodyResumeRechecksCurrentWorldBeforeEffects)
{
    Add("ParentTraining");
    Add("SlowTraining");
    auto context = Context(1);
    std::optional<TReactionPayload> result;
    io_.ExpectValue(1, 1, true);
    const auto point = Pause([&] {
        RunGeneratedReactionsSlowCounter(context, TReactionPayload::Create({}),
            TTriggerFixtureChange::TArguments{BindActionEntity<TCreature>(Player(0)), 1},
            [&](TReactionPayload value) { result = std::move(value); });
    });
    context->battle = nullptr;
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Read(1).Hands().Count(), 0);
    EXPECT_FALSE(result.has_value());
    context->battle = battle_.get();
    io_.ExpectValue(1, 3);
    point.Resume();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->Amount(), 16);
    EXPECT_EQ(Read(1).Hands().Count(), 4);
}

}
