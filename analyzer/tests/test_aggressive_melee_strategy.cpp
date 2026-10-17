#include <analyzer/aggressive_melee_strategy.h>
#include <analyzer/automated_interaction_system.h>

#include <pf2e_engine/actions/generated_source.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/rules.h>
#include <pf2e_engine/inventory/movement.h>
#include <pf2e_engine/random.h>

#include <gtest/gtest.h>

#include <list>
#include <initializer_list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

template <class T>
TIdCollection<T> Collection(std::initializer_list<T> values)
{
    TIdCollection<T> result;
    for (const auto& value : values) {
        result.Add(value);
    }
    return result;
}

TWeapon Weapon(int reach)
{
    return TWeapon::Create({
        .Name = "Declared weapon", .BaseDiceSize = 8, .Reach = reach,
        .Features = Collection<TFeatureSelection>({TFeatureSelection("abilities.WeaponStrike")}),
    });
}

TCreature Combatant(TIdCollection<TWeapon> weapons = {})
{
    return TCreature::Create({
        .Name = "Combatant", .Movement = 5,
        .Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 40})}),
        .Weapons = std::move(weapons),
        .Features = Collection<TFeatureSelection>({TFeatureSelection("abilities.BasicStrike"),
            TFeatureSelection("movement.MovementActions")}),
        .Actions = TResource(3),
    });
}

TAlternatives ActionChoices(const std::vector<TAbilityOption>& options, bool include_aggregate = true)
{
    auto choices = TAlternatives::Create<std::optional<TAbilityOption>>("next action");
    choices.AddAlternative("End turn", std::optional<TAbilityOption>{});
    for (const auto& option : options) {
        if (include_aggregate || option.DefinitionId != "abilities.UseStrike") {
            choices.AddAlternative(option.DefinitionId, std::optional<TAbilityOption>{option});
        }
    }
    return choices;
}

class TAggressiveMeleeStrategyTest : public testing::Test {
protected:
    void SetUp() override
    {
        battle_ = std::make_unique<TBattle>(TBattleMap(12, 12), &random_, io_, CreateAbilityRules());
        strategy_.BindBattle(*battle_);
    }

    TPlayer& Add(TCreature creature, int team, TPosition position)
    {
        const int id = static_cast<int>(creatures_.size());
        creatures_.push_back(std::move(creature));
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{team}, TPlayerId{id},
            "Player " + std::to_string(id), ""), position);
        return *battle_->GetIfPlayers([id](const TPlayer* player) { return player->GetId() == id; }).front();
    }

    std::optional<TAbilityOption> Selected(const TPlayer& actor, const TAlternatives& choices) const
    {
        return choices[strategy_.Decide(actor.GetId(), choices)].Get<std::optional<TAbilityOption>>();
    }
    TAggressiveMeleeStrategy strategy_;
    TAutomatedInteractionSystem io_{strategy_};
    TRandomGenerator random_{0};
    std::list<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
    TTransformator journal_{io_};
};

TEST_F(TAggressiveMeleeStrategyTest, LongReachUsesDeclaredDiagonalGeometryForAttacksAndMovement)
{
    auto& actor = Add(Combatant(Collection<TWeapon>({Weapon(3)})), 1, {1, 1});
    auto& enemy = Add(Combatant(), 2, {4, 4});
    auto choices = ActionChoices(battle_->AvailableActions(actor));
    ASSERT_TRUE(Selected(actor, choices));
    EXPECT_EQ(Selected(actor, choices)->DefinitionId, "movement.UseStride");

    auto movement = TAlternatives::Create<EMovementChoice>("choice");
    movement.AddAlternative("Continue", EMovementChoice::ContinueMovement);
    movement.AddAlternative("Finish", EMovementChoice::FinishMovement);
    EXPECT_EQ(strategy_.Decide(actor.GetId(), movement), 0U);

    const auto state = journal_.CurrentState();
    journal_.MovePlayer(&enemy, {3, 3});
    choices = ActionChoices(battle_->AvailableActions(actor));
    ASSERT_TRUE(Selected(actor, choices));
    EXPECT_EQ(Selected(actor, choices)->DefinitionId, "abilities.UseStrike");
    EXPECT_EQ(strategy_.Decide(actor.GetId(), movement), 1U);
    journal_.Undo(state);
    EXPECT_EQ(strategy_.Decide(actor.GetId(), movement), 0U);
}

TEST_F(TAggressiveMeleeStrategyTest, SourceBoundAttackMustReachAnEnemyAndAggregateAttackTakesPriority)
{
    auto& actor = Add(Combatant(Collection<TWeapon>({Weapon(1), Weapon(2)})), 1, {1, 1});
    auto& ally = Add(Combatant(), 1, {2, 1});
    auto& enemy = Add(Combatant(), 2, {3, 2});
    const auto options = battle_->AvailableActions(actor);
    const auto bound = Selected(actor, ActionChoices(options, false));
    ASSERT_TRUE(bound);
    EXPECT_EQ(bound->DefinitionId, "abilities.Strike");
    const auto source = FindAbilitySource<TWeapon>(*std::as_const(actor).GetCreature(), bound->Source);
    ASSERT_TRUE(source);
    EXPECT_EQ(source->Value->Reach(), 2);

    const auto aggregate = Selected(actor, ActionChoices(options));
    ASSERT_TRUE(aggregate);
    EXPECT_EQ(aggregate->DefinitionId, "abilities.UseStrike");

    auto targets = TAlternatives::Create<TPlayer*>("target");
    targets.AddAlternative("Ally", &ally);
    targets.AddAlternative("Actor", &actor);
    targets.AddAlternative("Enemy", &enemy);
    EXPECT_EQ(strategy_.Decide(actor.GetId(), targets), 2U);
}

TEST_F(TAggressiveMeleeStrategyTest, RemovedSourceFallsBackToMovementAndNoEnemiesEndsTheTurn)
{
    auto& actor = Add(Combatant(Collection<TWeapon>({Weapon(1)})), 1, {1, 1});
    auto& enemy = Add(Combatant(), 2, {2, 1});
    auto& ally = Add(Combatant(), 1, {1, 2});
    const auto choices = ActionChoices(battle_->AvailableActions(actor), false);
    ASSERT_TRUE(Selected(actor, choices));
    EXPECT_EQ(Selected(actor, choices)->DefinitionId, "abilities.Strike");
    const auto initial = journal_.CurrentState();
    TAstContext before_context;
    const auto before = actor.GetCreature()->GetAst(before_context);
    auto execution = journal_.ExecutionContext();
    execution.AssignGuarded(actor.GetCreature()->Weapons(), TIdCollection<TWeapon>{},
        TGuarded<TCreature>(*actor.GetCreature()), "weapons");
    ASSERT_TRUE(Selected(actor, choices));
    EXPECT_EQ(Selected(actor, choices)->DefinitionId, "movement.UseStride");
    execution.Invoke(TGuarded<TCreature>(*enemy.GetCreature()), &TCreature::Die);
    EXPECT_FALSE(Selected(actor, choices));

    auto targets = TAlternatives::Create<TPlayer*>("target");
    targets.AddAlternative("Ally", &ally);
    targets.AddAlternative("Defeated enemy", &enemy);
    EXPECT_THROW(strategy_.Decide(actor.GetId(), targets), std::logic_error);
    journal_.Undo(initial);
    TAstContext after_context;
    EXPECT_EQ(actor.GetCreature()->GetAst(after_context), before);
    ASSERT_TRUE(Selected(actor, choices));
    EXPECT_EQ(Selected(actor, choices)->DefinitionId, "abilities.Strike");
}

TEST_F(TAggressiveMeleeStrategyTest, AlliesDisableUnqualifiedReactiveStrikeButKeepShieldBlock)
{
    auto& actor = Add(Combatant(Collection<TWeapon>({Weapon(1)})), 1, {1, 1});
    auto reactions = TAlternatives::Create<size_t>("reaction");
    reactions.AddAlternative("Decline", size_t{0});
    reactions.AddAlternative("ReactiveStrike", size_t{1});
    EXPECT_EQ(strategy_.Decide(actor.GetId(), reactions), 1U);
    Add(Combatant(), 1, {1, 2});
    EXPECT_EQ(strategy_.Decide(actor.GetId(), reactions), 0U);
    reactions.AddAlternative("UseShieldBlock", size_t{2});
    EXPECT_EQ(strategy_.Decide(actor.GetId(), reactions), 2U);
}

}
