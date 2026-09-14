#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/inventory/generated_world.h>

#include "../test_lib/mock_dice_roller.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

class TWorldChoiceIo final : public IInteractionSystem {
public:
    void ExpectTarget(std::string name, bool suspend = false)
    {
        expected_.push_back({std::move(name), suspend});
    }

    void RequestChoice(int player_id, TAlternatives alternatives, TState state,
                       std::function<void(size_t)> on_choice) override
    {
        if (expected_.empty()) {
            throw std::logic_error("unexpected world action choice");
        }
        const bool suspend = expected_.front().Suspend;
        const auto index = ChooseAlternativeIndex(player_id, alternatives);
        if (suspend) {
            throw TSavepointStackUnwind(state,
                [on_choice = std::move(on_choice), index]() { on_choice(index); });
        }
        on_choice(index);
    }

    void HandleReactionTrigger(const TTriggerContext&, const TState&) override {}
    std::ostream& GameLog() override { return log_; }
    std::ostream& DevLog() override { return log_; }
    void Verify() const { EXPECT_TRUE(expected_.empty()); }
    const std::vector<std::string>& Candidates() const { return candidates_; }

protected:
    size_t ChooseAlternativeIndex(int player_id, const TAlternatives& alternatives) override
    {
        EXPECT_EQ(player_id, 0);
        if (expected_.empty()) {
            throw std::logic_error("unexpected world action alternative");
        }
        const auto expected = expected_.front();
        expected_.pop_front();
        candidates_.clear();
        std::optional<size_t> result;
        for (size_t index = 0; index < alternatives.Size(); ++index) {
            candidates_.push_back(alternatives[index].name);
            if (alternatives[index].name == expected.Name) {
                result = index;
            }
        }
        if (!result) {
            throw std::logic_error("expected target is absent: " + expected.Name);
        }
        return *result;
    }

private:
    struct TExpected {
        std::string Name;
        bool Suspend;
    };

    std::deque<TExpected> expected_;
    std::vector<std::string> candidates_;
    std::ostringstream log_;
};

TIdCollection<TFeatureSelection> WorldFeatures(const std::string& name)
{
    TIdCollection<TFeatureSelection> features;
    if (!name.empty()) {
        features.Add(TFeatureSelection("generated_world." + name));
    }
    return features;
}

THitPoints WorldHitpoints(int current)
{
    return THitPoints::Create({
        .Maximum = Statistic::Create({.Base = 100}),
        .Current = current,
    });
}

class TGeneratedWorldTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedWorldRules(*rules);
        RegisterGeneratedWorldRules(*rules);
        nlohmann::json map = {{"x_size", 8}, {"y_size", 8}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, std::move(rules));
        journal_ = std::make_unique<TTransformator>(io_);
    }

    void TearDown() override
    {
        rng_.Verify();
        io_.Verify();
    }

    TPlayer& Add(std::string name, TPosition position, int team, TCreature creature)
    {
        const int id = static_cast<int>(creatures_.size());
        creatures_.push_back(std::move(creature));
        rng_.ExpectCall(20, 10);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{team}, TPlayerId{id},
                                   std::move(name), ""), position);
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    TPlayer& Actor(int reach = 1, const std::string& feature = {}, bool class_feature = false)
    {
        const auto blade = CreateWorldBlade();
        TIdCollection<TWeapon> weapons;
        weapons.Add(TWeapon::Create({
            .Name = blade.Name(),
            .BaseDiceSize = blade.BaseDiceSize(),
            .DamageType = blade.DamageType(),
            .Category = blade.Category(),
            .Reach = reach,
            .Features = blade.Features(),
        }));
        return Add("Actor", {0, 0}, 0, TCreature::Create({
            .Level = 1,
            .Class = TClass::Create({
                .Features = WorldFeatures(class_feature ? feature : ""),
            }),
            .Hitpoints = WorldHitpoints(100),
            .Weapons = weapons,
            .Features = WorldFeatures(class_feature ? "" : feature),
            .Actions = TResource(3),
        }));
    }

    TPlayer& Participant(std::string name, TPosition position, int team = 1, int current = 100)
    {
        return Add(std::move(name), position, team, TCreature::Create({
            .Level = 1,
            .Hitpoints = WorldHitpoints(current),
        }));
    }

    TAbilityOption Option(TPlayer& actor, const std::string& name = "WorldProbe") const
    {
        const auto options = battle_->AvailableActions(actor);
        const auto found = std::ranges::find_if(options, [&](const TAbilityOption& option) {
            return option.DefinitionId == "generated_world." + name;
        });
        if (found == options.end()) {
            throw std::logic_error("missing world action option: " + name);
        }
        return *found;
    }

    TAbilityOption UncheckedOption(TPlayer& actor) const
    {
        const auto grants = CollectAbilities(*actor.GetCreature(), battle_->AbilityRules().Catalog);
        const auto& grant = grants.Grants().at(0);
        return {
            .ActorId = actor.GetId(),
            .ActorIdentity = &actor,
            .DefinitionId = grant.DefinitionId,
            .Source = grant.Source,
            .Provenance = grant.Provenance,
            .Action = battle_->AbilityRules().Executions.Require(grant.DefinitionId).Action,
            .Trigger = {},
        };
    }

    EAbilityExecutionResult ExecuteJournal(TPlayer& actor, const TAbilityOption& option)
    {
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
        throw std::logic_error("world action did not suspend");
    }

    TAstNode Snapshot() const
    {
        TAstContext context;
        auto node = TAstNode::MakeObject("generated_world_execution");
        AddOwnedObject(node, "journal", *journal_, context);
        AddOwnedObject(node, "battle", *battle_, context);
        return node;
    }

    TMockRng rng_;
    TWorldChoiceIo io_;
    std::deque<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> journal_;
};

TEST_F(TGeneratedWorldTest, NoEligibleTargetsHideTheActionAndRejectExecutionBeforePayment)
{
    auto& actor = Actor();
    Participant("Ally", {1, 0}, 0);
    Participant("Defeated", {0, 1}, 1, 0);
    Participant("Distant", {4, 4});
    const auto before = Snapshot();
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    const auto forged = UncheckedOption(actor);
    EXPECT_EQ(battle_->Execute(forged), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(ExecuteJournal(actor, forged), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedWorldTest, TargetFilterUsesChebyshevDistanceTeamsAndLivingParticipants)
{
    auto& actor = Actor();
    auto& diagonal = Participant("Diagonal", {1, 1});
    auto& straight = Participant("Straight", {1, 0});
    Participant("Ally", {0, 1}, 0);
    Participant("Distant", {3, 3});
    const auto before = Snapshot();
    const auto option = Option(actor);
    EXPECT_EQ(Snapshot(), before);
    io_.ExpectTarget("Diagonal");
    rng_.ExpectCall(6, 4);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(io_.Candidates(), (std::vector<std::string>{"Diagonal", "Straight"}));
    EXPECT_EQ(diagonal.GetCreature()->Hitpoints()->Current(), 96);
    EXPECT_EQ(straight.GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
}

TEST_F(TGeneratedWorldTest, StaleOptionsRecheckMovedOrDefeatedTargetsBeforePayment)
{
    auto& actor = Actor();
    auto& target = Participant("Target", {1, 0});
    const auto option = Option(actor);
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    journal_->MovePlayer(&target, {5, 0});
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
    journal_->Undo(initial);
    journal_->DealDamage(&target, 100);
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    EXPECT_EQ(ExecuteJournal(actor, option), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);

    io_.ExpectTarget("Target");
    rng_.ExpectCall(6, 2);
    EXPECT_EQ(ExecuteJournal(actor, option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 98);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedWorldTest, SuspendedChoiceRechecksLivePositionAndSupportsUndoAndReplay)
{
    auto& actor = Actor();
    auto& target = Participant("Target", {1, 0});
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectTarget("Target", true);
    rng_.ExpectCall(6, 4);
    const auto point = Suspend([&]() { ExecuteJournal(actor, Option(actor)); });
    TTransformator world_editor(io_);
    const auto edit_start = world_editor.CurrentState();
    world_editor.MovePlayer(&target, {5, 0});
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(rng_.RemainingCalls(), 1u);

    world_editor.Undo(edit_start);
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 96);
    point.Revert(*journal_);
    rng_.ExpectCall(6, 2);
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 98);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedWorldTest, WeaponReachIsSnapshottedWhileTargetCoordinatesRemainLive)
{
    auto& actor = Actor();
    auto& target = Participant("Target", {1, 0});
    const auto option = Option(actor);
    io_.ExpectTarget("Target", true);
    rng_.ExpectCall(6, 4);
    const auto first = Suspend([&]() { battle_->Execute(option); });
    auto context = journal_->ExecutionContext();
    const auto initial = context.GetState();
    auto weapon = context.Project(actor.GetCreature()->Weapons(), [](TIdCollection<TWeapon>& values) {
        return TGuarded<TWeapon>(*values.Find(TId<TWeapon>{0}));
    });
    auto reach = context.Project(weapon, [](TWeapon& value) { return value.Reach(); });
    context.AssignGuarded(reach, 3, weapon, "reach");
    const auto extended = context.GetState();
    journal_->MovePlayer(&target, {2, 0});
    EXPECT_EQ(battle_->AvailableActions(actor).size(), 1u);
    EXPECT_THROW(first.Resume(), TAbilityUnavailable);
    EXPECT_EQ(rng_.RemainingCalls(), 1u);

    context.Undo(extended);
    first.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 96);
    journal_->MovePlayer(&target, {2, 0});
    io_.ExpectTarget("Target", true);
    rng_.ExpectCall(6, 2);
    const auto second = Suspend([&]() { battle_->Execute(option); });
    context.AssignGuarded(reach, 1, weapon, "reach");
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    second.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 94);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 1);
    context.Undo(initial);
    EXPECT_EQ(target.GetPosition(), (TPosition{1, 0}));
    EXPECT_EQ(actor.GetCreature()->Weapons()->Find(TId<TWeapon>{0})->Reach(), 1);
}

TEST_F(TGeneratedWorldTest, TwoWolvesDoNotContributeUntilAnotherLivingAllyJoins)
{
    auto& actor = Actor(1, "TwoOtherAllies");
    auto& target = Participant("Target", {1, 0});
    Participant("Wolf B", {1, 1}, 0);
    Participant("Other enemy", {0, 1});
    Participant("Defeated ally", {2, 0}, 0, 0);
    const auto option = Option(actor);
    io_.ExpectTarget("Target");
    rng_.ExpectCall(6, 3);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 97);

    Participant("Wolf C", {2, 1}, 0);
    io_.ExpectTarget("Target");
    rng_.ExpectCall(6, 3);
    rng_.ExpectCall(4, 2);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 92);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 1);
}

TEST_F(TGeneratedWorldTest, EventDispatchReadsCurrentFeatureProvidersAndAllyPositions)
{
    auto& actor = Actor(1, "TwoOtherAllies", true);
    auto& target = Participant("Target", {1, 0});
    Participant("First ally", {1, 1}, 0);
    auto& other = Participant("Second ally", {2, 0}, 0);
    const auto option = Option(actor);
    io_.ExpectTarget("Target", true);
    rng_.ExpectCall(6, 3);
    const auto point = Suspend([&]() { battle_->Execute(option); });
    auto context = journal_->ExecutionContext();
    const auto initial = context.GetState();
    context.AssignGuarded(actor.GetCreature()->Class(), TClass::Create({}),
                          TGuarded<TCreature>(*actor.GetCreature()), "class");
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 97);
    context.Undo(initial);

    journal_->MovePlayer(&other, {5, 5});
    io_.ExpectTarget("Target");
    rng_.ExpectCall(6, 3);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 94);
    context.Undo(initial);
    io_.ExpectTarget("Target");
    rng_.ExpectCall(6, 3);
    rng_.ExpectCall(4, 2);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 89);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 0);
}

TEST_F(TGeneratedWorldTest, PrefixRequirementsGateFreshExecutionsWithoutCancellingPaidActions)
{
    auto& actor = Actor();
    auto& target = Participant("Target", {1, 0});
    const auto option = Option(actor);
    auto context = journal_->ExecutionContext();
    const auto initial = context.GetState();
    context.AssignGuarded(actor.GetCreature()->Level(), 0,
                          TGuarded<TCreature>(*actor.GetCreature()), "level");
    const auto rejected = Snapshot();
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(ExecuteJournal(actor, option), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
    EXPECT_EQ(Snapshot(), rejected);
    context.Undo(initial);

    io_.ExpectTarget("Target", true);
    rng_.ExpectCall(6, 4);
    const auto point = Suspend([&]() { battle_->Execute(option); });
    context.AssignGuarded(actor.GetCreature()->Level(), 0,
                          TGuarded<TCreature>(*actor.GetCreature()), "level");
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(rng_.RemainingCalls(), 1u);
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 96);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    context.Undo(initial);
    EXPECT_EQ(battle_->AvailableActions(actor).size(), 1u);
}

TEST_F(TGeneratedWorldTest, ResourceRequirementUsesPrepaymentBudgetAndAllowsResumeAfterSpending)
{
    auto& actor = Actor(1, "BudgetTraining");
    auto& target = Participant("Target", {2, 0});
    const auto option = Option(actor, "BudgetProbe");
    io_.ExpectTarget("Target", true);
    const auto point = Suspend([&]() { battle_->Execute(option); });
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Unavailable);
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 99);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
}

TEST_F(TGeneratedWorldTest, PositionBuiltinsUseCurrentCoordinatesInRequirementsAndBody)
{
    auto& actor = Actor(1, "CoordinateTraining");
    auto& target = Participant("Target", {2, 3});
    const auto option = Option(actor, "CoordinateProbe");
    const auto initial = journal_->CurrentState();
    journal_->MovePlayer(&actor, {0, 1});
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
    journal_->Undo(initial);

    io_.ExpectTarget("Target", true);
    const auto point = Suspend([&]() { battle_->Execute(option); });
    journal_->MovePlayer(&target, {4, 2});
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 58);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    journal_->Undo(initial);
    EXPECT_EQ(target.GetPosition(), (TPosition{2, 3}));
}

}
