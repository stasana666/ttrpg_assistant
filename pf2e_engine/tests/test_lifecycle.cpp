#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/transformation/transformator.h>

#include "test_lib/mock_dice_roller.h"
#include "test_lib/mock_interaction_system.h"
#include "test_lib/journal_setup.h"
#include "test_lib/state_diff.h"

#include <gtest/gtest.h>

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

TAstNode Snapshot(const TCreature& creature, const TTransformator& journal)
{
    TAstContext context;
    context.RegisterIdentity(&creature, "creature");
    auto node = TAstNode::MakeObject("lifecycle");
    AddOwnedObject(node, "creature", creature, context);
    AddOwnedObject(node, "journal", journal, context);
    return node;
}

TAstNode Snapshot(const TBattle& battle)
{
    TAstContext context;
    return battle.GetAst(context);
}

TEST(GeneratedLifecycleTest, RoundAndTurnBoundariesHaveSeparateJournaledBudgets)
{
    auto creature = TCreature::Create({
        .AttacksMade = 2, .Actions = TResource(7), .Reactions = TResource(4),
        .Flourishes = TResource(0), .Hands = TResource(2),
    });
    TMockInteractionSystem io;
    TTransformator journal(io);
    auto execution = journal.ExecutionContext();
    const auto initial = journal.CurrentState();
    const auto before = Snapshot(creature, journal);
    const auto fields_before = ObjectFields(creature);

    execution.Invoke(TGuarded<TCreature>(creature), &TCreature::BeginRound);
    EXPECT_EQ(creature.Flourishes()->Count(), 1);
    EXPECT_EQ(creature.Actions()->Count(), 7);
    EXPECT_EQ(creature.Reactions()->Count(), 4);
    EXPECT_EQ(creature.AttacksMade().Get(), 2);

    execution.Invoke(TGuarded<TCreature>(creature), &TCreature::BeginTurn);
    EXPECT_EQ(creature.Actions()->Count(), 3);
    EXPECT_EQ(creature.Reactions()->Count(), 1);
    EXPECT_EQ(creature.AttacksMade().Get(), 0);
    execution.SpendResource(creature.Flourishes(), 1, TGuarded<TCreature>(creature), "flourishes");
    execution.SpendResource(creature.Actions(), 2, TGuarded<TCreature>(creature), "actions");
    execution.Invoke(TGuarded<TCreature>(creature), &TCreature::RecordAttack, 2);
    const auto before_end = journal.CurrentState();
    const auto active_turn = Snapshot(creature, journal);

    execution.Invoke(TGuarded<TCreature>(creature), &TCreature::EndTurn);
    EXPECT_EQ(creature.Actions()->Count(), 0);
    EXPECT_EQ(creature.Reactions()->Count(), 1);
    EXPECT_EQ(creature.Flourishes()->Count(), 0);
    EXPECT_EQ(creature.AttacksMade().Get(), 0);
    EXPECT_EQ(creature.Hands()->Count(), 2);
    const auto completed_turn = Snapshot(creature, journal);

    journal.Undo(before_end);
    EXPECT_EQ(Snapshot(creature, journal), active_turn);
    execution.Invoke(TGuarded<TCreature>(creature), &TCreature::EndTurn);
    EXPECT_EQ(Snapshot(creature, journal), completed_turn);
    execution.Invoke(TGuarded<TCreature>(creature), &TCreature::BeginTurn);
    EXPECT_EQ(creature.Flourishes()->Count(), 0);
    EXPECT_EQ(creature.Actions()->Count(), 3);
    EXPECT_EQ(creature.Reactions()->Count(), 1);

    journal.Undo(initial);
    EXPECT_EQ(ObjectFields(creature), fields_before);
    EXPECT_EQ(Snapshot(creature, journal), before);
}

TEST(GeneratedLifecycleTest, FailedResourceResetRollsBackEarlierFieldsInTheMethod)
{
    auto creature = TCreature::Create({
        .AttacksMade = 2, .Actions = TResource(7), .Reactions = TResource(-1),
    });
    TMockInteractionSystem io;
    TTransformator journal(io);
    const auto before = Snapshot(creature, journal);
    auto execution = journal.ExecutionContext();
    EXPECT_THROW(execution.Invoke(TGuarded<TCreature>(creature), &TCreature::BeginTurn),
                 std::invalid_argument);
    EXPECT_EQ(Snapshot(creature, journal), before);
}

TEST(GeneratedLifecycleTest, DefaultsAndRestoredSpentFlourishRemainDistinct)
{
    const auto defaults = TCreature::Create({});
    EXPECT_EQ(defaults.Actions().Count(), 0);
    EXPECT_EQ(defaults.Reactions().Count(), 0);
    EXPECT_EQ(defaults.Flourishes().Count(), 1);
    const auto spent = TCreature::Create({.Flourishes = TResource(0)});
    EXPECT_EQ(spent.Flourishes().Count(), 0);
}

class TBattleLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        rules->Catalog.Register({"lifecycle.Probe", EAbilityKind::Action, {}});
        rules->Catalog.Register({"lifecycle.Flourish", EAbilityKind::Action, {}});
        rules->Catalog.Register({"lifecycle.Training", EAbilityKind::Feature,
                                 {"lifecycle.Probe", "lifecycle.Flourish"}});
        const auto body = [this](std::shared_ptr<TActionContext> context) {
            journal_ = context->transformator;
            if (on_action_) {
                on_action_(std::move(context));
            }
        };
        rules->Executions.Register("lifecycle.Probe", std::make_shared<TAction>(
            body, TAction::TBoundResources{}, "Probe"));
        rules->Executions.Register("lifecycle.Flourish", std::make_shared<TAction>(
            body, TAction::TBoundResources{
                {
                    .Key = "self.actions", .Count = 1,
                    .Read = [](const TPlayer& actor) -> const TResource& {
                        return actor.GetCreature()->Actions();
                    },
                    .Write = [](TPlayer& actor) { return actor.GetCreature()->Actions(); },
                },
                {
                    .Key = "self.flourishes", .Count = 1,
                    .Read = [](const TPlayer& actor) -> const TResource& {
                        return actor.GetCreature()->Flourishes();
                    },
                    .Write = [](TPlayer& actor) { return actor.GetCreature()->Flourishes(); },
                },
            }, "Flourish"));
        battle_ = std::make_unique<TBattle>(TBattleMap(3, 2), &rng_, io_, std::move(rules));
        for (int id = 0; id < 3; ++id) {
            TIdCollection<TFeatureSelection> features;
            features.Add(TFeatureSelection("lifecycle.Training"));
            creatures_.push_back(TCreature::Create({
                .Hitpoints = THitPoints::Create({
                    .Maximum = Statistic::Create({.Base = 20}), .Current = 20,
                }),
                .Features = std::move(features), .AttacksMade = 2,
                .Reactions = TResource(id == 2 ? 1 : 0), .Flourishes = TResource(0),
            }));
            rng_.ExpectCall(20, 10);
            battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{id == 0 ? 0 : 1},
                                      TPlayerId{id}, "Participant", ""), {id, 0});
        }
    }

    TPlayer& Player(int id)
    {
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    TCreature& Creature(int id) { return *Player(id).GetCreature(); }

    TMockInteractionSystem io_;
    TMockRng rng_;
    std::deque<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
    TTransformator* journal_ = nullptr;
    TAction::FBody on_action_;
};

TEST_F(TBattleLifecycleTest, RoundResetsEveryoneWhileReactionsWaitForTheirOwnTurn)
{
    io_.AddCheckCallback([&] {
        EXPECT_EQ(Creature(0).Actions()->Count(), 3);
        EXPECT_EQ(Creature(0).Reactions()->Count(), 1);
        EXPECT_EQ(Creature(1).Reactions()->Count(), 0);
        EXPECT_EQ(Creature(2).Reactions()->Count(), 1);
        for (int id = 0; id < 3; ++id) {
            EXPECT_EQ(Creature(id).Flourishes()->Count(), 1);
        }
    });
    io_.ExpectChoice(0, "next action", "Flourish");
    io_.ExpectChoice(0, "next action", "End of turn");
    io_.AddCheckCallback([&] {
        EXPECT_EQ(Creature(0).Actions()->Count(), 0);
        EXPECT_EQ(Creature(0).Reactions()->Count(), 1);
        EXPECT_EQ(Creature(0).Flourishes()->Count(), 0);
        EXPECT_EQ(Creature(1).Actions()->Count(), 3);
        EXPECT_EQ(Creature(1).Reactions()->Count(), 1);
        EXPECT_EQ(Creature(2).AttacksMade().Get(), 2);
        ReduceResource(*journal_, Creature(1).Reactions(), 1);
    });
    io_.ExpectChoice(1, "next action", "Flourish");
    io_.ExpectChoice(1, "next action", "End of turn");
    io_.ExpectChoice(2, "next action", "Flourish");
    io_.ExpectChoice(2, "next action", "End of turn");
    io_.AddCheckCallback([&] {
        for (int id = 0; id < 3; ++id) {
            EXPECT_EQ(Creature(id).Flourishes()->Count(), 1);
            EXPECT_EQ(Creature(id).AttacksMade().Get(), 0);
        }
        EXPECT_EQ(Creature(1).Reactions()->Count(), 0);
        EXPECT_EQ(Creature(2).Reactions()->Count(), 1);
    });
    EXPECT_THROW(battle_->StartBattle(), TTooManyCallsError);
    io_.Verify();
    rng_.Verify();
}

TEST_F(TBattleLifecycleTest, TurnBoundariesResetBudgetsAndWholeRoundUndoes)
{
    on_action_ = [&](std::shared_ptr<TActionContext> context) {
        auto* journal = context->transformator;
        auto execution = journal->ExecutionContext();
        execution.Invoke(TGuarded<TCreature>(Creature(0)), &TCreature::RecordAttack, 2);
        ReduceResource(*journal, Creature(0).Reactions(), 1);
    };
    std::optional<TState> checkpoint;
    std::optional<TAstNode> before;
    io_.ExpectChoice(0, "next action", "Flourish");
    io_.AddCheckCallback([&] {
        checkpoint = journal_->CurrentState();
        before = Snapshot(*battle_);
    });
    io_.ExpectChoice(0, "next action", "End of turn");
    io_.ExpectChoice(1, "next action", "End of turn");
    io_.ExpectChoice(2, "next action", "End of turn");
    EXPECT_THROW(battle_->StartBattle(), TTooManyCallsError);
    EXPECT_EQ(Creature(0).Actions()->Count(), 3);
    EXPECT_EQ(Creature(0).Reactions()->Count(), 1);
    ASSERT_TRUE(checkpoint.has_value());
    ASSERT_TRUE(before.has_value());
    EXPECT_NE(Snapshot(*battle_), *before);
    journal_->Undo(*checkpoint);
    const auto restored = Snapshot(*battle_);
    EXPECT_EQ(restored, *before) << restored.DiffWith(*before);
    EXPECT_EQ(Creature(0).Actions()->Count(), 2);
    EXPECT_EQ(Creature(0).Flourishes()->Count(), 0);
    EXPECT_EQ(Creature(0).Reactions()->Count(), 0);
    EXPECT_EQ(Creature(0).AttacksMade().Get(), 2);
    io_.Verify();
    rng_.Verify();
}

}
