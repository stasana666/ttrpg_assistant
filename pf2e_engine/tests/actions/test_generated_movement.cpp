#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/reaction_window.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/inventory/movement.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <any>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

class TGeneratedMovementTest : public TBattleFixture {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterMovementRules(*rules);
        StartBattle(std::move(rules), 10);
    }

    TPlayer& Add(TPosition position, std::initializer_list<std::string> features = {},
                 int speed = 3, int team = -1, bool reactive = false,
                 int reach = 1, bool prone = false, int actions = 3)
    {
        const int id = static_cast<int>(creatures_.size());
        TCreature::TInitialValues initial;
        initial.Name = "Actor " + std::to_string(id);
        initial.Movement = speed;
        initial.Hitpoints = THitPoints::Create({
            .Maximum = Statistic::Create({.Base = 100}), .Current = 100,
        });
        initial.ArmorClassBaseline = TStatisticBaseline::Create({.IsFixed = true, .Value = 15});
        TIdCollection<TWeapon> weapons;
        weapons.Add(TWeapon::Create({.Name = "Blade", .BaseDiceSize = 6,
            .AttackBonusBaseline = TStatisticBaseline::Create({.IsFixed = true, .Value = 10}),
            .DamageModifierBaseline = TStatisticBaseline::Create({.IsFixed = true, .Value = 0}),
            .DamageType = EDamageType::Slashing, .Reach = reach}));
        initial.Weapons = std::move(weapons);
        initial.Features = Features(features);
        if (reactive) {
            initial.Class = TClass::Create({.Name = "Reactive Fighter",
                .Features = Features({"movement.ReactiveStrikeTraining"})});
        }
        if (prone) {
            TVariantMap<EConditionKind, TCondition> conditions;
            conditions.Set(TConditionProne{});
            initial.Conditions = std::move(conditions);
        }
        initial.Actions = TResource(actions);
        initial.Reactions = TResource(4);
        initial.Flourishes = TResource(1);
        return Join(TCreature::Create(initial), team < 0 ? id : team, position);
    }

    void Next(TPosition destination, bool suspend = false)
    {
        io_.ExpectValue(0, EMovementChoice::ContinueMovement);
        io_.ExpectValue(0, destination, suspend);
    }

    void Stop() { io_.ExpectValue(0, EMovementChoice::FinishMovement); }

    void React(int natural = 5, bool suspend = false)
    {
        io_.ExpectName(1, "reaction", "ReactiveStrike", suspend);
        io_.ExpectName(1, "source", "Blade");
        rng_.ExpectCall(20, natural);
        if (natural >= 5) {
            rng_.ExpectCall(6, 2);
        }
    }
};

TEST_F(TGeneratedMovementTest, DiagonalCostsPersistAcrossStridesAndResetAtOwnTurnEnd)
{
    Add({1, 1}, {"movement.MovementActions"});
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    Next({2, 2});
    Stop();
    EXPECT_EQ(Execute("movement.UseStride"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(0).DiagonalSteps(), 1);
    Next({3, 3});
    Next({4, 3});
    EXPECT_EQ(Execute("movement.UseStride"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Player(0).GetPosition(), (TPosition{4, 3}));
    EXPECT_EQ(Read(0).DiagonalSteps(), 2);
    EXPECT_EQ(Read(0).Actions().Count(), 1);
    const auto before_end = Snapshot();
    const auto end = journal_->CurrentState();
    auto execution = journal_->ExecutionContext();
    execution.Invoke(TGuarded<TCreature>(Creature(0)), &TCreature::EndTurn);
    EXPECT_EQ(Read(0).DiagonalSteps(), 0);
    journal_->Undo(end);
    EXPECT_EQ(Snapshot(), before_end);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedMovementTest, StepDoesNotProvokeAndItsDiagonalCountsForLaterMovement)
{
    Add({1, 1}, {"movement.MovementActions"});
    Add({1, 2}, {}, 3, 1, true, 3);
    io_.ExpectValue(0, TPosition{2, 2});
    EXPECT_EQ(Execute("movement.UseStep"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(0).DiagonalSteps(), 1);
    io_.ExpectValue(0, TPosition{2, 3}, false, [](const TAlternatives& alternatives) {
        for (size_t index = 0; index < alternatives.Size(); ++index) {
            const auto* position = std::any_cast<TPosition>(&alternatives[index].value);
            ASSERT_NE(position, nullptr);
            EXPECT_NE(*position, (TPosition{3, 3}));
        }
    });
    EXPECT_EQ(Execute("movement.UseStep"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(0).Actions().Count(), 1);
    EXPECT_EQ(Read(0).Hitpoints().Current(), 100);
    EXPECT_EQ(Read(1).Reactions().Count(), 4);
}

TEST_F(TGeneratedMovementTest, StepWithoutLegalDestinationFailsBeforePayment)
{
    Add({0, 0}, {"movement.MovementActions"});
    const auto option = Option("movement.UseStep");
    Add({1, 0});
    Add({0, 1});
    Add({1, 1});
    const auto before = Snapshot();
    EXPECT_FALSE(Available("movement.UseStep"));
    EXPECT_EQ(Execute(option), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(Read(0).Actions().Count(), 3);
    EXPECT_EQ(Snapshot(), before);
    EXPECT_EQ(io_.Requests(), 0U);
}

TEST_F(TGeneratedMovementTest, SlowOrProneCreaturesCannotStepBeforePayment)
{
    Add({0, 0}, {"movement.MovementActions"}, 1);
    Add({2, 0}, {"movement.MovementActions"}, 3, 1, false, 1, true);
    EXPECT_FALSE(Available("movement.UseStep"));
    EXPECT_FALSE(Available("movement.UseStep", 1));
    EXPECT_FALSE(Available("movement.UseStride", 1));
    EXPECT_EQ(Read(0).Actions().Count(), 3);
    EXPECT_EQ(Read(1).Actions().Count(), 3);
}

TEST_F(TGeneratedMovementTest, ClassGrantedReactionOccursBeforeDepartureWithoutMapOrDisruption)
{
    Add({0, 0}, {"movement.MovementActions"}, 4);
    Add({0, 1}, {}, 3, 1, true, 4);
    auto execution = journal_->ExecutionContext();
    execution.Invoke(TGuarded<TCreature>(Creature(1)), &TCreature::RecordAttack, 2);
    ASSERT_EQ(Read(1).Weapons().Find(TId<TWeapon>{0})->Features().Size(), 0U);
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    Next({1, 0});
    io_.ExpectName(1, "reaction", "ReactiveStrike", false, [&](const TAlternatives&) {
        EXPECT_EQ(Player(0).GetPosition(), (TPosition{0, 0}));
        EXPECT_EQ(Read(0).Actions().Count(), 2);
    });
    io_.ExpectName(1, "source", "Blade");
    rng_.ExpectCall(20, 5);
    rng_.ExpectCall(6, 2);
    Next({2, 0});
    Stop();
    EXPECT_EQ(Execute("movement.UseStride"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Player(0).GetPosition(), (TPosition{2, 0}));
    EXPECT_EQ(Read(0).Hitpoints().Current(), 98);
    EXPECT_EQ(Read(1).AttacksMade(), 2);
    EXPECT_EQ(Read(1).Reactions().Count(), 3);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedMovementTest, MerelyEnteringReachDoesNotOpenAReactionWindow)
{
    Add({0, 0}, {"movement.MovementActions"}, 1);
    Add({2, 0}, {}, 3, 1, true);
    Next({1, 0});
    EXPECT_EQ(Execute("movement.UseStride"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Player(0).GetPosition(), (TPosition{1, 0}));
    EXPECT_EQ(Read(1).Reactions().Count(), 4);
    EXPECT_EQ(Read(0).Hitpoints().Current(), 100);
}

TEST_F(TGeneratedMovementTest, ACriticalReactiveStrikeDoesNotDisruptDeparture)
{
    Add({0, 0}, {"movement.MovementActions"}, 1);
    Add({0, 1}, {}, 3, 1, true);
    Next({1, 0});
    React(20);
    EXPECT_EQ(Execute("movement.UseStride"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Player(0).GetPosition(), (TPosition{1, 0}));
    EXPECT_EQ(Read(0).Hitpoints().Current(), 96);
    EXPECT_EQ(Read(1).AttacksMade(), 0);
}

TEST_F(TGeneratedMovementTest, AStrideStoppedWithoutLeavingItsSquareTriggersAtTheEnd)
{
    Add({0, 0}, {"movement.MovementActions"});
    Add({0, 1}, {}, 3, 1, true);
    Stop();
    React(1);
    EXPECT_EQ(Execute("movement.UseStride"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Player(0).GetPosition(), (TPosition{0, 0}));
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    EXPECT_EQ(Read(1).Reactions().Count(), 3);
}

TEST_F(TGeneratedMovementTest, ACompletelyBlockedStrideStillOpensItsEndingWindow)
{
    Add({0, 0}, {"movement.MovementActions"});
    Add({0, 1}, {}, 3, 1, true);
    Add({1, 0});
    Add({1, 1});
    React(1);
    EXPECT_EQ(Execute("movement.UseStride"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Player(0).GetPosition(), (TPosition{0, 0}));
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    EXPECT_EQ(Read(1).Reactions().Count(), 3);
}

TEST_F(TGeneratedMovementTest, StandRemovesProneBeforeItsCriticalMovementReaction)
{
    Add({0, 0}, {"movement.MovementActions"}, 3, 0, false, 1, true);
    Add({0, 1}, {}, 3, 1, true);
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectName(1, "reaction", "ReactiveStrike", false, [&](const TAlternatives&) {
        EXPECT_FALSE(Read(0).Conditions().Has(EConditionKind::Prone));
        EXPECT_EQ(Read(0).ArmorClass(), 15);
    });
    io_.ExpectName(1, "source", "Blade");
    rng_.ExpectCall(20, 20);
    rng_.ExpectCall(6, 2);
    EXPECT_EQ(Execute("movement.UseStand"), EAbilityExecutionResult::Applied);
    EXPECT_FALSE(Read(0).Conditions().Has(EConditionKind::Prone));
    EXPECT_EQ(Read(0).Hitpoints().Current(), 96);
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    EXPECT_EQ(Read(1).AttacksMade(), 0);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedMovementTest, ReactiveStrikeMayBeAcceptedAgainstAnAlliedCreature)
{
    Add({0, 0}, {"movement.MovementActions"}, 1, 0);
    Add({0, 1}, {}, 3, 0, true);
    Next({1, 0});
    React();
    EXPECT_EQ(Execute("movement.UseStride"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(0).Hitpoints().Current(), 98);
    EXPECT_EQ(Read(1).Reactions().Count(), 3);
}

TEST_F(TGeneratedMovementTest, DecliningAnEarlySquareLeavesTheSameStrideReactionAvailable)
{
    Add({0, 0}, {"movement.MovementActions"}, 2);
    Add({0, 1}, {}, 3, 1, true, 3);
    Next({1, 0});
    io_.ExpectName(1, "reaction", "Decline");
    Next({2, 0});
    React(1);
    EXPECT_EQ(Execute("movement.UseStride"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Reactions().Count(), 3);
}

TEST_F(TGeneratedMovementTest, SuddenChargeHasFreshStrideScopesAndSharedDiagonalParityAcrossReplay)
{
    Add({0, 0}, {"movement.SuddenCharge"}, 2, 0, false, 3);
    Add({0, 1}, {}, 3, 1, true, 3);
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    Next({1, 1});
    React(1);
    Stop();
    Next({2, 2});
    io_.ExpectName(1, "reaction", "ReactiveStrike", true);
    const auto point = Pause([&] { Execute("movement.UseSuddenCharge"); });
    EXPECT_EQ(Read(0).Actions().Count(), 1);
    EXPECT_EQ(Read(0).Flourishes().Count(), 0);
    EXPECT_EQ(Read(0).DiagonalSteps(), 1);
    io_.ExpectName(1, "source", "Blade");
    rng_.ExpectCall(20, 1);
    io_.ExpectValue(0, false);
    point.Resume();
    EXPECT_EQ(Player(0).GetPosition(), (TPosition{2, 2}));
    EXPECT_EQ(Read(0).DiagonalSteps(), 2);
    EXPECT_EQ(Read(0).AttacksMade(), 0);
    EXPECT_EQ(Read(1).Reactions().Count(), 2);
    const auto completed = Snapshot();
    point.Revert(*journal_);
    io_.ExpectName(1, "source", "Blade");
    rng_.ExpectCall(20, 1);
    io_.ExpectValue(0, false);
    point.Resume();
    EXPECT_EQ(Snapshot(), completed);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedMovementTest, SuddenChargeCanStartWithoutATargetAndStrikeAnEnemyAfterBothStrides)
{
    Add({0, 0}, {"movement.SuddenCharge"}, 1);
    Add({3, 0});
    ASSERT_TRUE(Available("movement.UseSuddenCharge"));
    Next({1, 0});
    Next({2, 0});
    io_.ExpectValue(0, true);
    io_.ExpectName(0, "target", "Actor 1");
    io_.ExpectName(0, "source", "Blade");
    rng_.ExpectCall(20, 5);
    rng_.ExpectCall(6, 2);
    EXPECT_EQ(Execute("movement.UseSuddenCharge"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(0).Actions().Count(), 1);
    EXPECT_EQ(Read(0).AttacksMade(), 1);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 98);
}

TEST_F(TGeneratedMovementTest, SuddenChargeFinalStrikeOffersEnemiesOnly)
{
    Add({0, 0}, {"movement.SuddenCharge"}, 1, 0);
    Add({2, 1}, {}, 3, 0);
    Add({2, 0});
    Next({1, 0});
    Stop();
    io_.ExpectValue(0, true);
    io_.ExpectName(0, "target", "Actor 2", false, [](const TAlternatives& alternatives) {
        EXPECT_EQ(alternatives.Size(), 1U);
    });
    io_.ExpectName(0, "source", "Blade");
    rng_.ExpectCall(20, 5);
    rng_.ExpectCall(6, 2);
    EXPECT_EQ(Execute("movement.UseSuddenCharge"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 100);
    EXPECT_EQ(Read(2).Hitpoints().Current(), 98);
}

TEST_F(TGeneratedMovementTest, SuddenChargeAndViciousSwingSpendTheSameFlourishPool)
{
    Add({0, 0}, {"movement.SuddenCharge", "abilities.ViciousSwing"}, 1, 0, false, 1, false, 5);
    Add({1, 0});
    const auto initial = journal_->CurrentState();
    io_.ExpectName(0, "target", "Actor 1");
    io_.ExpectName(0, "source", "Blade");
    rng_.ExpectCall(20, 5);
    rng_.ExpectCall(6, 2);
    rng_.ExpectCall(6, 2);
    EXPECT_EQ(Execute("abilities.UseViciousSwing"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(0).Actions().Count(), 3);
    EXPECT_FALSE(Available("movement.UseSuddenCharge"));
    journal_->Undo(initial);
    Stop();
    Stop();
    io_.ExpectValue(0, false);
    EXPECT_EQ(Execute("movement.UseSuddenCharge"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(0).Actions().Count(), 3);
    EXPECT_FALSE(Available("abilities.UseViciousSwing"));
}

TEST_F(TGeneratedMovementTest, StepRechecksOccupancyAndOriginalPositionAcrossRepeatedResume)
{
    Add({1, 1}, {"movement.MovementActions"});
    Add({4, 4});
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectValue(0, TPosition{2, 1}, true);
    const auto point = Pause([&] { Execute("movement.UseStep"); });
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    editor.MovePlayer(&Player(1), {2, 1});
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Player(0).GetPosition(), (TPosition{1, 1}));
    editor.Undo(edit);
    editor.MovePlayer(&Player(0), {2, 2});
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    editor.Undo(edit);
    point.Resume();
    EXPECT_EQ(Player(0).GetPosition(), (TPosition{2, 1}));
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    const auto completed = Snapshot();
    point.Revert(*journal_);
    point.Resume();
    EXPECT_EQ(Snapshot(), completed);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedMovementTest, RemovingAReactionGrantDuringWeaponChoiceStopsItsEffectsUntilRestore)
{
    Add({0, 0}, {"movement.MovementActions"}, 2);
    Add({0, 1}, {}, 3, 1, true, 3);
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    Next({1, 0});
    io_.ExpectName(1, "reaction", "ReactiveStrike");
    io_.ExpectName(1, "source", "Blade", true);
    const auto point = Pause([&] { Execute("movement.UseStride"); });
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    auto execution = editor.ExecutionContext();
    auto selected_class = Creature(1).Class();
    auto features = execution.Project(selected_class, [](TClass& value) { return value.Features(); });
    execution.AssignGuarded(features, TIdCollection<TFeatureSelection>{}, selected_class, "features");
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Read(0).Hitpoints().Current(), 100);
    EXPECT_EQ(Player(0).GetPosition(), (TPosition{0, 0}));
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    EXPECT_EQ(Read(1).Reactions().Count(), 3);
    editor.Undo(edit);
    rng_.ExpectCall(20, 1);
    Stop();
    point.Resume();
    EXPECT_EQ(Player(0).GetPosition(), (TPosition{1, 0}));
    const auto completed = Snapshot();
    point.Revert(*journal_);
    rng_.ExpectCall(20, 1);
    Stop();
    point.Resume();
    EXPECT_EQ(Snapshot(), completed);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

}
