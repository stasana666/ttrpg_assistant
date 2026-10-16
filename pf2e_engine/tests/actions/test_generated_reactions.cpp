#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/reaction_window.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/inventory/generated_reactions.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

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

TEST_F(TGeneratedReactionsTest, ImmediatePayloadAndExplicitDeclinePayOnlyAcceptedReaction)
{
    Add("ParentTraining");
    Add("ImmediateTraining");
    Add("ImmediateTraining");
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectName(1, "reaction", "ImmediateCounter");
    io_.ExpectName(2, "reaction", "Decline");
    EXPECT_EQ(Execute("ChangeWithWindow"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(0).Hands().Count(), 17);
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    EXPECT_EQ(Read(1).Reactions().Count(), 3);
    EXPECT_EQ(Read(2).Reactions().Count(), 4);
    EXPECT_EQ(Read(1).Hands().Count(), 1);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedReactionsTest, TwoSuspensionsReplayTypedPayloadAndParentTailWithoutDoublePayment)
{
    Add("ParentTraining");
    Add("SlowTraining");
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectName(1, "reaction", "SlowCounter");
    io_.ExpectValue(1, 2, true);
    const auto first = Pause([&] { Execute("ChangeWithWindow"); });
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    EXPECT_EQ(Read(1).Reactions().Count(), 3);
    io_.ExpectValue(1, 4, true);
    first.Revert(*journal_);
    const auto second = Pause([&] { first.Resume(); });
    EXPECT_EQ(Read(1).Hands().Count(), 2);
    second.Revert(*journal_);
    second.Resume();
    EXPECT_EQ(Read(1).Hands().Count(), 6);
    EXPECT_EQ(Read(0).Hands().Count(), 14);
    const auto completed = Snapshot();
    first.Revert(*journal_);
    io_.ExpectValue(1, 4, true);
    const auto replay = Pause([&] { first.Resume(); });
    replay.Revert(*journal_);
    replay.Resume();
    EXPECT_EQ(Snapshot(), completed);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedReactionsTest, SourceRequirementsReadLiveStateBeforeReactionPayment)
{
    Add("ParentTraining");
    Add("", true, true);
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    io_.ExpectName(1, "reaction", "BoundCounter", true);
    const auto offer = Pause([&] { Execute("ChangeWithWindow"); });
    EXPECT_EQ(Read(1).Reactions().Count(), 4);
    SetWeapon(editor, 0, true);
    EXPECT_THROW(offer.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Read(1).Reactions().Count(), 4);
    EXPECT_EQ(Read(1).Hands().Count(), 0);
    editor.Undo(edit);
    io_.ExpectValue(1, 0);
    offer.Resume();
    EXPECT_EQ(Read(0).Hands().Count(), 16);
    EXPECT_EQ(Read(1).Reactions().Count(), 3);
}

TEST_F(TGeneratedReactionsTest, MissingBoundSourceAndEmptyLeadingSourceChoiceDoNotOfferReaction)
{
    Add("ParentTraining");
    Add("BoundTraining");
    Add("SelectedTraining");
    EXPECT_EQ(Execute("ChangeWithWindow"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(io_.Requests(), 0U);
    EXPECT_EQ(Read(0).Hands().Count(), 20);
    EXPECT_EQ(Read(1).Reactions().Count(), 4);
    EXPECT_EQ(Read(2).Reactions().Count(), 4);
}

TEST_F(TGeneratedReactionsTest, BoundSourceValuesRemainSnapshotsAcrossSuspension)
{
    Add("ParentTraining");
    Add("", true, true);
    io_.ExpectName(1, "reaction", "BoundCounter");
    io_.ExpectValue(1, 0, true);
    const auto point = Pause([&] { Execute("ChangeWithWindow"); });
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    SetWeapon(editor, 8);
    point.Resume();
    EXPECT_EQ(Read(0).Hands().Count(), 16);
    EXPECT_EQ(Read(1).Weapons().Find(TId<TWeapon>{0})->BaseDiceSize(), 8);
    editor.Undo(edit);
}

TEST_F(TGeneratedReactionsTest, SelectedWeaponNeedsNoGrantAndRemovalAbortsUntilRestore)
{
    Add("ParentTraining");
    Add("SelectedTraining", true);
    EXPECT_EQ(Read(1).Weapons().Find(TId<TWeapon>{0})->Features().Size(), 0U);
    io_.ExpectName(1, "reaction", "SelectedCounter");
    io_.ExpectName(1, "source", "Blade");
    io_.ExpectValue(1, 1, true);
    const auto point = Pause([&] { Execute("ChangeWithWindow"); });
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    RemoveWeapon(editor);
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Read(1).Hands().Count(), 0);
    EXPECT_EQ(Read(1).Reactions().Count(), 3);
    editor.Undo(edit);
    point.Resume();
    EXPECT_EQ(Read(0).Hands().Count(), 15);
    EXPECT_EQ(Read(1).Hands().Count(), 1);
}

TEST_F(TGeneratedReactionsTest, ReactionFinishesAfterParentGrantRemovalThenParentRejectsItsTail)
{
    Add("ParentTraining");
    Add("SlowTraining");
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectName(1, "reaction", "SlowCounter");
    io_.ExpectValue(1, 1, true);
    const auto point = Pause([&] { Execute("ChangeWithWindow"); });
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    RemoveFeatures(editor, 0);
    io_.ExpectValue(1, 3);
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Read(1).Hands().Count(), 4);
    EXPECT_EQ(Read(1).Reactions().Count(), 3);
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    EXPECT_EQ(Read(0).Hands().Count(), 0);
    editor.Undo(edit);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedReactionsTest, ReactionFinishesAfterParentDeathAndPreservesPaidHistory)
{
    Add("ParentTraining");
    Add("KillingTraining");
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectName(1, "reaction", "KillingCounter");
    io_.ExpectValue(1, 2, true);
    const auto point = Pause([&] { Execute("ChangeWithWindow"); });
    EXPECT_EQ(Read(0).Hitpoints().Current(), 0);
    EXPECT_TRUE(Read(0).IsDead());
    EXPECT_EQ(Read(1).Hands().Count(), 2);
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Read(1).Hands().Count(), 4);
    EXPECT_EQ(Read(1).Reactions().Count(), 3);
    EXPECT_EQ(Read(0).Actions().Count(), 0);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedReactionsTest, OneActivitySharesLimitAcrossCellsAndReactionDefinitions)
{
    Add("ParentTraining");
    Add("LimitedTraining");
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectValue(0, TPosition{1, 0});
    io_.ExpectName(1, "reaction", "LimitedSecond");
    io_.ExpectValue(0, TPosition{2, 0});
    io_.ExpectValue(0, TPosition{3, 0});
    EXPECT_EQ(Execute("WalkOnce"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Player(0).GetPosition(), (TPosition{3, 0}));
    EXPECT_EQ(Read(0).Hands().Count(), 10);
    EXPECT_EQ(Read(1).Hands().Count(), 10);
    EXPECT_EQ(Read(1).Reactions().Count(), 3);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedReactionsTest, SubordinateActivityCallsGetFreshScopesAcrossSuspension)
{
    Add("ParentTraining");
    Add("LimitedTraining");
    io_.ExpectValue(0, TPosition{1, 0});
    io_.ExpectName(1, "reaction", "LimitedFirst");
    io_.ExpectValue(0, TPosition{2, 0});
    io_.ExpectValue(0, TPosition{3, 0}, true);
    const auto point = Pause([&] { Execute("WalkTwice"); });
    EXPECT_EQ(Player(0).GetPosition(), (TPosition{2, 0}));
    io_.ExpectName(1, "reaction", "LimitedSecond");
    io_.ExpectValue(0, TPosition{4, 0});
    point.Resume();
    EXPECT_EQ(Player(0).GetPosition(), (TPosition{4, 0}));
    EXPECT_EQ(Read(0).Hands().Count(), 29);
    EXPECT_EQ(Read(1).Hands().Count(), 11);
    EXPECT_EQ(Read(1).Reactions().Count(), 2);
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    const auto completed = Snapshot();
    point.Revert(*journal_);
    io_.ExpectName(1, "reaction", "LimitedSecond");
    io_.ExpectValue(0, TPosition{4, 0});
    point.Resume();
    EXPECT_EQ(Snapshot(), completed);
}

TEST_F(TGeneratedReactionsTest, MultipleSubscriptionsHaveIndependentTriggerLimitKeys)
{
    Add("ParentTraining");
    Add("LimitedTraining");
    const auto first = rules_->Reactions.Find<TTriggerFixtureMove>("generated_reactions.LimitedFirst");
    const auto second = rules_->Reactions.Find<TTriggerFixtureOtherMove>("generated_reactions.LimitedFirst");
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first->LimitKey, second->LimitKey);
    io_.ExpectName(1, "reaction", "LimitedFirst");
    io_.ExpectName(1, "reaction", "LimitedFirst");
    EXPECT_EQ(Execute("OtherWindow"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(0).Hands().Count(), 18);
    EXPECT_EQ(Read(1).Reactions().Count(), 2);
}

TEST_F(TGeneratedReactionsTest, NestedDifferentTriggerReturnsThroughBothReactionFrames)
{
    Add("ParentTraining");
    Add("NestedTraining");
    Add("InnerTraining");
    io_.ExpectName(1, "reaction", "NestedCounter");
    io_.ExpectName(2, "reaction", "InnerCounter");
    io_.ExpectValue(2, 2, true);
    const auto point = Pause([&] { Execute("ChangeWithWindow"); });
    point.Resume();
    EXPECT_EQ(Read(0).Hands().Count(), 18);
    EXPECT_EQ(Read(1).Hands().Count(), 1);
    EXPECT_EQ(Read(2).Hands().Count(), 2);
    EXPECT_EQ(Read(1).Reactions().Count(), 3);
    EXPECT_EQ(Read(2).Reactions().Count(), 3);
}

TEST_F(TGeneratedReactionsTest, NativeBodyEntryRejectsReadonlyForeignAndMissingReturnChannel)
{
    Add("ParentTraining");
    Add("SlowTraining");
    auto context = Context(1);
    const auto before = Snapshot();
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
    EXPECT_EQ(Snapshot(), before);
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
