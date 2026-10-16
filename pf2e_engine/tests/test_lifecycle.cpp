#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/transformation/transformator.h>

#include "test_lib/mock_dice_roller.h"
#include "test_lib/mock_interaction_system.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

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

TAstNode Snapshot(const TTaskScheduler& scheduler, const TTransformator& journal)
{
    TAstContext context;
    context.RegisterIdentity(&scheduler, "scheduler");
    auto node = TAstNode::MakeObject("scheduled_lifecycle");
    AddOwnedObject(node, "scheduler", scheduler, context);
    AddOwnedObject(node, "journal", journal, context);
    return node;
}

TEST(GeneratedLifecycleTest, RemovedTaskOrderSurvivesRepeatedUndoAndDispatch)
{
    TMockInteractionSystem io;
    TTransformator journal(io);
    TTaskScheduler scheduler;
    const TEvent event{.type = EEvent::OnTurnEnd, .context = {.player = nullptr}};
    std::vector<int> calls;
    for (int label : {10, 20, 30}) {
        journal.AddTask(&scheduler, {
            .events_before_call = {event},
            .callback = [label, &calls] {
                calls.push_back(label);
                return false;
            },
        });
    }
    const auto checkpoint = journal.CurrentState();
    const auto before = Snapshot(scheduler, journal);
    for (int replay = 0; replay < 3; ++replay) {
        calls.clear();
        scheduler.TriggerEvent(event, journal);
        EXPECT_EQ(calls, (std::vector<int>{10, 20, 30}));
        journal.Undo(checkpoint);
        const auto restored = Snapshot(scheduler, journal);
        EXPECT_EQ(restored, before) << restored.DiffWith(before);
    }
}

TEST(GeneratedLifecycleTest, InvalidTaskRestoreDoesNotChangeSchedulerOrJournal)
{
    TMockInteractionSystem io;
    TTransformator journal(io);
    TTaskScheduler scheduler;
    const TTask task{
        .events_before_call = {{.type = EEvent::OnTurnEnd, .context = {.player = nullptr}}},
        .callback = [] { return false; },
    };
    const auto id = journal.AddTask(&scheduler, task);
    const auto before = Snapshot(scheduler, journal);
    EXPECT_THROW(scheduler.RestoreTask(id, task, 0), std::invalid_argument);
    EXPECT_THROW(scheduler.RestoreTask(id + 1, task, 0), std::invalid_argument);
    EXPECT_THROW(scheduler.RestoreTask(id + 1, task, 2), std::invalid_argument);
    EXPECT_THROW(scheduler.RestoreTask(id + 1, TTask{}, 0), std::invalid_argument);
    EXPECT_EQ(Snapshot(scheduler, journal), before);
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
    const auto json_before = creature.ToJson();

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
    EXPECT_EQ(creature.ToJson(), json_before);
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
    auto creature = TCreature::Create({.Flourishes = TResource(0)});
    const auto json = creature.ToJson();
    EXPECT_EQ(json.at("flourishes"), 0);
    TGameObjectFactory factory;
    const auto restored = TCreature::Restore(json, factory);
    EXPECT_EQ(restored.ToJson(), json);
    EXPECT_EQ(restored.Id(), creature.Id());
    EXPECT_EQ(restored.Flourishes().Count(), 0);
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
        nlohmann::json map = {{"x_size", 3}, {"y_size", 2}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, std::move(rules));
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
        journal_->ReduceResource(Creature(1).Reactions(), 1);
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

TEST_F(TBattleLifecycleTest, ScheduledTurnEffectsPrecedeResetsAndWholeRoundUndoes)
{
    int end_calls = 0;
    int start_calls = 0;
    on_action_ = [&](std::shared_ptr<TActionContext> context) {
        auto* actor = &Player(0);
        auto* journal = context->transformator;
        auto execution = journal->ExecutionContext();
        execution.Invoke(TGuarded<TCreature>(Creature(0)), &TCreature::RecordAttack, 2);
        journal->ReduceResource(Creature(0).Reactions(), 1);
        journal->AddTask(context->scheduler, {
            .events_before_call = {{.type = EEvent::OnTurnEnd, .context = {.player = actor}}},
            .callback = [&] {
                ++end_calls;
                EXPECT_EQ(Creature(0).Actions()->Count(), 2);
                EXPECT_EQ(Creature(0).AttacksMade().Get(), 2);
                return false;
            },
        });
        journal->AddTask(context->scheduler, {
            .events_before_call = {{.type = EEvent::OnTurnStart, .context = {.player = actor}}},
            .callback = [&] {
                ++start_calls;
                EXPECT_EQ(Creature(0).Actions()->Count(), 0);
                EXPECT_EQ(Creature(0).Reactions()->Count(), 0);
                EXPECT_EQ(Creature(0).AttacksMade().Get(), 0);
                EXPECT_EQ(Creature(0).Flourishes()->Count(), 1);
                return false;
            },
        });
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
    EXPECT_EQ(end_calls, 1);
    EXPECT_EQ(start_calls, 1);
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
