#include <pf2e_engine/inventory/movement.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>

#include <any>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

struct TMover {
    std::vector<std::string> Features = {};
    int Speed = 3;
    int Team = -1;
    bool Reactive = false;
    int Reach = 1;
    bool Prone = false;
    int Actions = 3;
};

TPick At(int x, int y)
{
    return TPick::Value(TPosition{x, y});
}

const TPick kContinue = TPick::Value(EMovementChoice::ContinueMovement);
const TPick kFinish = TPick::Value(EMovementChoice::FinishMovement);
const TPick kReact(1, "ReactiveStrike");
const TPick kBlade(1, "Blade");
const std::vector<std::string> kMoves = {"movement.MovementActions"};

class TGeneratedMovementTest : public TBattleFixture {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterMovementRules(*rules);
        StartBattle(std::move(rules), 10);
    }

    TPlayer& Add(TPosition position, const TMover& mover = {})
    {
        const int id = static_cast<int>(creatures_.size());
        TCreature::TInitialValues initial;
        initial.Name = "Actor " + std::to_string(id);
        initial.Movement = mover.Speed;
        initial.Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 100}), .Current = 100});
        initial.ArmorClassBaseline = Fixed(15);
        TIdCollection<TWeapon> weapons;
        weapons.Add(TWeapon::Create({.Name = "Blade", .BaseDiceSize = 6, .AttackBonusBaseline = Fixed(10),
            .DamageModifierBaseline = Fixed(0), .DamageType = EDamageType::Slashing, .Reach = mover.Reach}));
        initial.Weapons = std::move(weapons);
        auto features = Features({});
        for (const auto& feature : mover.Features) {
            features.Add(TFeatureSelection(feature));
        }
        initial.Features = std::move(features);
        if (mover.Reactive) {
            initial.Class = TClass::Create({.Name = "Reactive Fighter",
                .Features = Features({"movement.ReactiveStrikeTraining"})});
        }
        if (mover.Prone) {
            TVariantMap<EConditionKind, TCondition> conditions;
            conditions.Set(TConditionProne{});
            initial.Conditions = std::move(conditions);
        }
        initial.Actions = TResource(mover.Actions);
        initial.Reactions = TResource(4);
        initial.Flourishes = TResource(1);
        return Join(TCreature::Create(initial), mover.Team < 0 ? id : mover.Team, position);
    }

    TPick Unmoved(TPick pick, TPosition position)
    {
        return pick.WhenOffered([this, position](const TAlternatives&) {
            EXPECT_EQ(Player(0).GetPosition(), position);
        });
    }
};

TEST_F(TGeneratedMovementTest, DiagonalCostsPersistAcrossStridesAndResetAtTurnEnd)
{
    Add({1, 1}, {.Features = kMoves});
    Act("movement.UseStride", 0, {.Choose = {kContinue, At(2, 2), kFinish},
        .Changes = {{"Actor 0.position", "TPosition{2,2}"}, {"Actor 0.diagonal_steps", 1}, {"Actor 0.actions.count", 2}}});
    Act("movement.UseStride", 0, {.Choose = {kContinue, At(3, 3), kContinue, At(4, 3)},
        .Changes = {{"Actor 0.position", "TPosition{4,3}"}, {"Actor 0.diagonal_steps", 2}, {"Actor 0.actions.count", 1}}});
    journal_->ExecutionContext().Invoke(TGuarded<TCreature>(Creature(0)), &TCreature::EndTurn);
    EXPECT_EQ(Read(0).DiagonalSteps(), 0);
}

TEST_F(TGeneratedMovementTest, StepDoesNotProvokeAndItsDiagonalCountsForLaterMovement)
{
    Add({1, 1}, {.Features = kMoves});
    Add({1, 2}, {.Team = 1, .Reactive = true, .Reach = 3});
    Act("movement.UseStep", 0, {.Choose = {At(2, 2)},
        .Changes = {{"Actor 0.position", "TPosition{2,2}"}, {"Actor 0.diagonal_steps", 1}, {"Actor 0.actions.count", 2}}});
    const auto no_second_diagonal = At(2, 3).WhenOffered([](const TAlternatives& alternatives) {
        for (size_t index = 0; index < alternatives.Size(); ++index) {
            EXPECT_NE(std::any_cast<TPosition>(alternatives[index].value), (TPosition{3, 3}));
        }
    });
    Act("movement.UseStep", 0, {.Choose = {no_second_diagonal},
        .Changes = {{"Actor 0.position", "TPosition{2,3}"}, {"Actor 0.actions.count", 1}}});
}

TEST_F(TGeneratedMovementTest, StepNeedsALegalDestinationEnoughSpeedAndNoProneCondition)
{
    Add({0, 0}, {.Features = kMoves});
    const auto option = Option("movement.UseStep");
    Add({1, 0});
    Add({0, 1});
    Add({1, 1});
    Add({5, 5}, {.Features = kMoves, .Speed = 1});
    Add({7, 7}, {.Features = kMoves, .Prone = true});
    EXPECT_FALSE(Available("movement.UseStep"));
    Act(option, {.Result = EAbilityExecutionResult::Unavailable});
    EXPECT_FALSE(Available("movement.UseStep", 4));
    EXPECT_FALSE(Available("movement.UseStep", 5));
    EXPECT_FALSE(Available("movement.UseStride", 5));
}

TEST_F(TGeneratedMovementTest, ReactiveStrikeHitsBeforeDepartureWithoutPenaltyOrDisruption)
{
    Add({0, 0}, {.Features = kMoves, .Speed = 4});
    Add({0, 1}, {.Team = 1, .Reactive = true, .Reach = 4});
    journal_->ExecutionContext().Invoke(TGuarded<TCreature>(Creature(1)), &TCreature::RecordAttack, 2);
    Act("movement.UseStride", 0, {.Choose = {kContinue, At(1, 0), Unmoved(kReact, {0, 0}), kBlade, kContinue, At(2, 0),
                                             kFinish},
        .Rolls = {{20, 5}, {6, 2}},
        .Changes = {{"Actor 0.position", "TPosition{2,0}"}, {"Actor 0.hitpoints.current", 98},
                    {"Actor 0.actions.count", 2}, {"Actor 1.reactions.count", 3}}});
}

TEST_F(TGeneratedMovementTest, EnteringReachDoesNotOpenAReactionWindow)
{
    Add({0, 0}, {.Features = kMoves, .Speed = 1});
    Add({2, 0}, {.Team = 1, .Reactive = true});
    Act("movement.UseStride", 0, {.Choose = {kContinue, At(1, 0)},
        .Changes = {{"Actor 0.position", "TPosition{1,0}"}, {"Actor 0.actions.count", 2}}});
}

TEST_F(TGeneratedMovementTest, ACriticalReactiveStrikeDoesNotDisruptTheMove)
{
    Add({0, 0}, {.Features = kMoves, .Speed = 1});
    Add({0, 1}, {.Team = 1, .Reactive = true});
    Act("movement.UseStride", 0, {.Choose = {kContinue, At(1, 0), kReact, kBlade}, .Rolls = {{20, 20}, {6, 2}},
        .Changes = {{"Actor 0.position", "TPosition{1,0}"}, {"Actor 0.hitpoints.current", 96},
                    {"Actor 0.actions.count", 2}, {"Actor 1.reactions.count", 3}}});
}

TEST_F(TGeneratedMovementTest, StoppedOrBlockedStridesStillOpenTheEndingWindow)
{
    Add({0, 0}, {.Features = kMoves});
    Add({0, 1}, {.Team = 1, .Reactive = true});
    Check(Option("movement.UseStride"), {.Choose = {kFinish, kReact, kBlade}, .Rolls = {{20, 1}},
        .Changes = {{"Actor 0.actions.count", 2}, {"Actor 1.reactions.count", 3}}});
    Add({1, 0});
    Add({1, 1});
    Act("movement.UseStride", 0, {.Choose = {kReact, kBlade}, .Rolls = {{20, 1}},
        .Changes = {{"Actor 0.actions.count", 2}, {"Actor 1.reactions.count", 3}}});
}

TEST_F(TGeneratedMovementTest, StandRemovesProneBeforeItsReactiveStrike)
{
    Add({0, 0}, {.Features = kMoves, .Team = 0, .Prone = true});
    Add({0, 1}, {.Team = 1, .Reactive = true});
    const auto standing = kReact.WhenOffered([this](const TAlternatives&) {
        EXPECT_FALSE(Read(0).Conditions().Has(EConditionKind::Prone));
        EXPECT_EQ(Read(0).ArmorClass(), 15);
    });
    Act("movement.UseStand", 0, {.Choose = {standing, kBlade}, .Rolls = {{20, 20}, {6, 2}},
        .Changes = {{"Actor 0.conditions.Prone", "<removed>"}, {"Actor 0.hitpoints.current", 96},
                    {"Actor 0.actions.count", 2}, {"Actor 1.reactions.count", 3}}});
}

TEST_F(TGeneratedMovementTest, ReactiveStrikeMayBeUsedAgainstAnAlly)
{
    Add({0, 0}, {.Features = kMoves, .Speed = 1, .Team = 0});
    Add({0, 1}, {.Team = 0, .Reactive = true});
    Act("movement.UseStride", 0, {.Choose = {kContinue, At(1, 0), kReact, kBlade}, .Rolls = {{20, 5}, {6, 2}},
        .Changes = {{"Actor 0.position", "TPosition{1,0}"}, {"Actor 0.hitpoints.current", 98},
                    {"Actor 0.actions.count", 2}, {"Actor 1.reactions.count", 3}}});
}

TEST_F(TGeneratedMovementTest, DecliningAtOneSquareKeepsTheReactionForTheNextSquare)
{
    Add({0, 0}, {.Features = kMoves, .Speed = 2});
    Add({0, 1}, {.Team = 1, .Reactive = true, .Reach = 3});
    Act("movement.UseStride", 0, {.Choose = {kContinue, At(1, 0), TPick(1, "Decline"), kContinue, At(2, 0), kReact,
                                             kBlade},
        .Rolls = {{20, 1}},
        .Changes = {{"Actor 0.position", "TPosition{2,0}"}, {"Actor 0.actions.count", 2},
                    {"Actor 1.reactions.count", 3}}});
}

TEST_F(TGeneratedMovementTest, SuddenChargeStridesHaveFreshReactionScopesAndShareDiagonalParity)
{
    Add({0, 0}, {.Features = {"movement.SuddenCharge"}, .Speed = 2, .Team = 0, .Reach = 3});
    Add({0, 1}, {.Team = 1, .Reactive = true, .Reach = 3});
    Act("movement.UseSuddenCharge", 0, {
        .Choose = {kContinue, At(1, 1), kReact, kBlade, kFinish, kContinue, At(2, 2), kReact, kBlade,
                   TPick::Value(false)},
        .Rolls = {{20, 1}, {20, 1}},
        .Changes = {{"Actor 0.position", "TPosition{2,2}"}, {"Actor 0.diagonal_steps", 2}, {"Actor 0.actions.count", 1},
                    {"Actor 0.flourishes.count", 0}, {"Actor 1.reactions.count", 2}}});
}

TEST_F(TGeneratedMovementTest, SuddenChargeCanStartWithoutATargetAndStrikeAfterBothStrides)
{
    Add({0, 0}, {.Features = {"movement.SuddenCharge"}, .Speed = 1});
    Add({3, 0});
    Act("movement.UseSuddenCharge", 0, {
        .Choose = {kContinue, At(1, 0), kContinue, At(2, 0), TPick::Value(true), "Actor 1", "Blade"},
        .Rolls = {{20, 5}, {6, 2}},
        .Changes = {{"Actor 0.position", "TPosition{2,0}"}, {"Actor 0.actions.count", 1},
                    {"Actor 0.flourishes.count", 0}, {"Actor 0.attacks_made", 1}, {"Actor 1.hitpoints.current", 98}}});
}

TEST_F(TGeneratedMovementTest, SuddenChargeFinalStrikeOffersEnemiesOnly)
{
    Add({0, 0}, {.Features = {"movement.SuddenCharge"}, .Speed = 1, .Team = 0});
    Add({2, 1}, {.Team = 0});
    Add({2, 0});
    Act("movement.UseSuddenCharge", 0, {
        .Choose = {kContinue, At(1, 0), kFinish, TPick::Value(true), "Actor 2", "Blade"},
        .Rolls = {{20, 5}, {6, 2}},
        .Changes = {{"Actor 0.position", "TPosition{1,0}"}, {"Actor 0.actions.count", 1},
                    {"Actor 0.flourishes.count", 0}, {"Actor 0.attacks_made", 1}, {"Actor 2.hitpoints.current", 98}}});
    EXPECT_EQ(io_.Candidates("target"), (std::vector<std::string>{"Actor 2"}));
}

TEST_F(TGeneratedMovementTest, SuddenChargeAndViciousSwingSpendTheSameFlourish)
{
    Add({0, 0}, {.Features = {"movement.SuddenCharge", "abilities.ViciousSwing"}, .Speed = 1, .Team = 0,
                 .Actions = 5});
    Add({1, 0});
    const auto start = journal_->CurrentState();
    Act("abilities.UseViciousSwing", 0, {.Choose = {"Actor 1", "Blade"}, .Rolls = {{20, 5}, {6, 2}, {6, 2}},
        .Changes = {{"Actor 0.actions.count", 3}, {"Actor 0.flourishes.count", 0}, {"Actor 0.attacks_made", 2},
                    {"Actor 1.hitpoints.current", 96}}});
    EXPECT_FALSE(Available("movement.UseSuddenCharge"));
    journal_->Undo(start);
    Act("movement.UseSuddenCharge", 0, {.Choose = {kFinish, kFinish, TPick::Value(false)},
        .Changes = {{"Actor 0.actions.count", 3}, {"Actor 0.flourishes.count", 0}}});
    EXPECT_FALSE(Available("abilities.UseViciousSwing"));
}

}
