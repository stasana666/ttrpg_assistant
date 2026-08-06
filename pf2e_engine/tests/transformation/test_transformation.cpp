#include <gtest/gtest.h>

#include <pf2e_engine/transformation/transformation.h>
#include <pf2e_engine/transformation/transformator.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/common/resource.h>
#include <pf2e_engine/effect_manager.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <cpp_config.h>

#include <mock_interaction_system.h>

#include <filesystem>

const std::filesystem::path kPathToData{kRootDirPath + "/pf2e_engine/data"};

class TransformationTest : public ::testing::Test {
protected:
    TVariantMap<EConditionKind, TCondition> conditions_;
};

TEST_F(TransformationTest, ChangeConditionAppliesValue) {
    EXPECT_FALSE(conditions_.Has(EConditionKind::Frightened));

    TChangeCondition change(&conditions_, EConditionKind::Frightened, 3);

    ASSERT_TRUE(conditions_.Has(EConditionKind::Frightened));
    EXPECT_EQ(conditions_.Get<TConditionFrightened>()->Value, 3);
}

TEST_F(TransformationTest, ChangeConditionUndoRestoresPreviousValue) {
    TChangeCondition setup(&conditions_, EConditionKind::Frightened, 2);
    ASSERT_TRUE(conditions_.Has(EConditionKind::Frightened));
    EXPECT_EQ(conditions_.Get<TConditionFrightened>()->Value, 2);

    TChangeCondition change(&conditions_, EConditionKind::Frightened, 5);
    ASSERT_TRUE(conditions_.Has(EConditionKind::Frightened));
    EXPECT_EQ(conditions_.Get<TConditionFrightened>()->Value, 5);

    change.Undo();
    ASSERT_TRUE(conditions_.Has(EConditionKind::Frightened));
    EXPECT_EQ(conditions_.Get<TConditionFrightened>()->Value, 2);
}

TEST_F(TransformationTest, ChangeConditionUndoFromZero) {
    EXPECT_FALSE(conditions_.Has(EConditionKind::MultipleAttackPenalty));

    TChangeCondition change(&conditions_, EConditionKind::MultipleAttackPenalty, 5);
    ASSERT_TRUE(conditions_.Has(EConditionKind::MultipleAttackPenalty));
    EXPECT_EQ(conditions_.Get<TConditionMultipleAttackPenalty>()->Value, 5);

    change.Undo();
    EXPECT_FALSE(conditions_.Has(EConditionKind::MultipleAttackPenalty));
}

TEST(ChangeResourceTest, AddResourceAppliesValue) {
    TResource resource;

    EXPECT_EQ(resource.Count(), 0);

    TChangeResource change(&resource, 5);

    EXPECT_EQ(resource.Count(), 5);
}

TEST(ChangeResourceTest, AddResourceUndoRemovesValue) {
    TResource resource;

    TChangeResource change(&resource, 5);
    EXPECT_EQ(resource.Count(), 5);

    change.Undo();
    EXPECT_EQ(resource.Count(), 0);
}

TEST(ChangeResourceTest, ReduceResourceAppliesValue) {
    TResource resource(10);

    EXPECT_EQ(resource.Count(), 10);

    TChangeResource change(&resource, -3);

    EXPECT_EQ(resource.Count(), 7);
}

TEST(ChangeResourceTest, ReduceResourceUndoRestoresValue) {
    TResource resource(10);

    TChangeResource change(&resource, -4);
    EXPECT_EQ(resource.Count(), 6);

    change.Undo();
    EXPECT_EQ(resource.Count(), 10);
}

TEST(ChangeResourceTest, ReduceBelowZeroUndoRestoresOriginalCount) {
    TResource resource(3);

    // Reducing by more than the current count clamps to 0 inside TResource.
    TChangeResource change(&resource, -5);
    EXPECT_EQ(resource.Count(), 0);

    // Undo must restore the real previous count (3), not replay the delta (+5).
    change.Undo();
    EXPECT_EQ(resource.Count(), 3);
}

TEST(ChangeResourceTest, ZeroDeltaDoesNothing) {
    TResource resource(5);

    TChangeResource change(&resource, 0);
    EXPECT_EQ(resource.Count(), 5);

    change.Undo();
    EXPECT_EQ(resource.Count(), 5);
}

class TransformatorTest : public ::testing::Test {
protected:
    using FsPath = std::filesystem::path;
    using FsDirEntry = std::filesystem::directory_entry;
    using FsRecursiveIterator = std::filesystem::recursive_directory_iterator;

    void SetUp() override {
        for (const FsDirEntry& dir_entry : FsRecursiveIterator(kPathToData)) {
            if (dir_entry.is_regular_file() && dir_entry.path().extension() == ".json") {
                factory_.AddSource(dir_entry.path());
            }
        }

        auto warrior_id = TGameObjectIdManager::Instance().Register("warrior");
        creature_ = std::make_unique<TCreature>(factory_.Create<TCreature>(warrior_id));
        transformator_ = std::make_unique<TTransformator>(mock_io_);
    }

    TGameObjectFactory factory_;
    TMockInteractionSystem mock_io_;
    std::unique_ptr<TCreature> creature_;
    std::unique_ptr<TTransformator> transformator_;
};

TEST_F(TransformatorTest, ChangeConditionViaTransformator) {
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 0);

    transformator_->ChangeCondition(creature_.get(), EConditionKind::Frightened, 3);
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 3);
}

TEST_F(TransformatorTest, ChangeConditionUndoViaTransformator) {
    TState initial_state = transformator_->CurrentState();

    transformator_->ChangeCondition(creature_.get(), EConditionKind::Frightened, 3);
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 3);

    transformator_->Undo(initial_state);
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 0);
}

TEST_F(TransformatorTest, AddResourceViaTransformator) {
    EXPECT_EQ(creature_->ResourceFor(EResourceKind::Action)->Count(), 0);

    transformator_->AddResource(creature_->ResourceFor(EResourceKind::Action), 5);
    EXPECT_EQ(creature_->ResourceFor(EResourceKind::Action)->Count(), 5);
}

TEST_F(TransformatorTest, ReduceResourceViaTransformator) {
    transformator_->AddResource(creature_->ResourceFor(EResourceKind::Action), 10);

    transformator_->ReduceResource(creature_->ResourceFor(EResourceKind::Action), 3);
    EXPECT_EQ(creature_->ResourceFor(EResourceKind::Action)->Count(), 7);
}

TEST_F(TransformatorTest, ResourceUndoViaTransformator) {
    transformator_->AddResource(creature_->ResourceFor(EResourceKind::Action), 10);

    TState initial_state = transformator_->CurrentState();

    transformator_->ReduceResource(creature_->ResourceFor(EResourceKind::Action), 3);
    EXPECT_EQ(creature_->ResourceFor(EResourceKind::Action)->Count(), 7);

    transformator_->AddResource(creature_->ResourceFor(EResourceKind::Action), 2);
    EXPECT_EQ(creature_->ResourceFor(EResourceKind::Action)->Count(), 9);

    transformator_->Undo(initial_state);
    EXPECT_EQ(creature_->ResourceFor(EResourceKind::Action)->Count(), 10);
}

TEST_F(TransformatorTest, MixedTransformationsUndo) {
    TState initial_state = transformator_->CurrentState();

    transformator_->ChangeCondition(creature_.get(), EConditionKind::Frightened, 2);
    transformator_->AddResource(creature_->ResourceFor(EResourceKind::Action), 5);
    transformator_->ChangeCondition(creature_.get(), EConditionKind::MultipleAttackPenalty, 5);

    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 2);
    EXPECT_EQ(creature_->ResourceFor(EResourceKind::Action)->Count(), 5);
    EXPECT_EQ(creature_->Get(EConditionKind::MultipleAttackPenalty), 5);

    transformator_->Undo(initial_state);

    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 0);
    EXPECT_EQ(creature_->ResourceFor(EResourceKind::Action)->Count(), 0);
    EXPECT_EQ(creature_->Get(EConditionKind::MultipleAttackPenalty), 0);
}

class EffectTransformationTest : public ::testing::Test {
protected:
    using FsPath = std::filesystem::path;
    using FsDirEntry = std::filesystem::directory_entry;
    using FsRecursiveIterator = std::filesystem::recursive_directory_iterator;

    void SetUp() override {
        for (const FsDirEntry& dir_entry : FsRecursiveIterator(kPathToData)) {
            if (dir_entry.is_regular_file() && dir_entry.path().extension() == ".json") {
                factory_.AddSource(dir_entry.path());
            }
        }

        auto warrior_id = TGameObjectIdManager::Instance().Register("warrior");
        creature_ = std::make_unique<TCreature>(factory_.Create<TCreature>(warrior_id));
        player_ = std::make_unique<TPlayer>(creature_.get(), TPlayerTeam{0}, TPlayerId{0}, "Test Player", "");
        effect_manager_ = std::make_unique<TEffectManager>();
        transformator_ = std::make_unique<TTransformator>(mock_io_);
    }

    TGameObjectFactory factory_;
    TMockInteractionSystem mock_io_;
    std::unique_ptr<TCreature> creature_;
    std::unique_ptr<TPlayer> player_;
    std::unique_ptr<TEffectManager> effect_manager_;
    std::unique_ptr<TTransformator> transformator_;
};

TEST_F(EffectTransformationTest, AddEffectInsertsValue) {
    TAddEffect add(effect_manager_.get(), player_.get(), EConditionKind::Frightened, 3);

    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 3);
}

TEST_F(EffectTransformationTest, AddEffectUndoRemovesValue) {
    TAddEffect add(effect_manager_.get(), player_.get(), EConditionKind::Frightened, 3);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 3);

    add.Undo();
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 0);
}

TEST_F(EffectTransformationTest, RemoveEffectErasesValue) {
    effect_manager_->InsertValue(player_.get(), EConditionKind::Frightened, 3);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 3);

    TRemoveEffect remove(effect_manager_.get(), player_.get(), EConditionKind::Frightened, 3);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 0);
}

TEST_F(EffectTransformationTest, RemoveEffectUndoRestoresValue) {
    effect_manager_->InsertValue(player_.get(), EConditionKind::Frightened, 3);

    TRemoveEffect remove(effect_manager_.get(), player_.get(), EConditionKind::Frightened, 3);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 0);

    remove.Undo();
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 3);
}

TEST_F(EffectTransformationTest, EffectManagerAddEffectWithTransformator) {
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 0);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 0);

    TState initial_state = transformator_->CurrentState();

    effect_manager_->AddEffect(TPlayerConditionSet{
        .player = player_.get(),
        .condition = EConditionKind::Frightened,
        .value = 3,
    }, *transformator_);

    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 3);
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 3);

    transformator_->Undo(initial_state);

    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 0);
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 0);
}

TEST_F(EffectTransformationTest, MultipleEffectsWithUndo) {
    TState initial_state = transformator_->CurrentState();

    effect_manager_->AddEffect(TPlayerConditionSet{
        .player = player_.get(),
        .condition = EConditionKind::Frightened,
        .value = 2,
    }, *transformator_);

    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 2);
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 2);

    effect_manager_->AddEffect(TPlayerConditionSet{
        .player = player_.get(),
        .condition = EConditionKind::Frightened,
        .value = 5,
    }, *transformator_);

    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 5);
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 5);

    transformator_->Undo(initial_state);

    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 0);
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 0);
}


class TaskTransformationTest : public ::testing::Test {
protected:
    void SetUp() override {
        scheduler_ = std::make_unique<TTaskScheduler>();
        transformator_ = std::make_unique<TTransformator>(mock_io_);
    }

    TMockInteractionSystem mock_io_;
    std::unique_ptr<TTaskScheduler> scheduler_;
    std::unique_ptr<TTransformator> transformator_;
};

TEST_F(TaskTransformationTest, AddTaskUndoRemovesTask) {
    int callback_count = 0;
    TTask task{
        .events_before_call = {{EEvent::OnTurnEnd, TEventContext{nullptr}}},
        .callback = [&callback_count]() {
            ++callback_count;
            return false;
        }
    };

    TAddTask add_task(scheduler_.get(), std::move(task));

    scheduler_->TriggerEvent({EEvent::OnTurnEnd, TEventContext{nullptr}}, *transformator_);
    EXPECT_EQ(callback_count, 1);

    add_task.Undo();

    scheduler_->TriggerEvent({EEvent::OnTurnEnd, TEventContext{nullptr}}, *transformator_);
    EXPECT_EQ(callback_count, 1);
}

TEST_F(TaskTransformationTest, RemoveTaskUndoRestoresTask) {
    int callback_count = 0;
    TTask task{
        .events_before_call = {{EEvent::OnTurnEnd, TEventContext{nullptr}}},
        .callback = [&callback_count]() {
            ++callback_count;
            return false;
        }
    };

    TTaskId task_id = scheduler_->AddTaskWithId(std::move(task));

    TTask task_copy{
        .events_before_call = {{EEvent::OnTurnEnd, TEventContext{nullptr}}},
        .callback = [&callback_count]() {
            ++callback_count;
            return false;
        }
    };

    TRemoveTask remove_task(scheduler_.get(), task_id, std::move(task_copy), 0);

    scheduler_->TriggerEvent({EEvent::OnTurnEnd, TEventContext{nullptr}}, *transformator_);
    EXPECT_EQ(callback_count, 0);

    remove_task.Undo();

    scheduler_->TriggerEvent({EEvent::OnTurnEnd, TEventContext{nullptr}}, *transformator_);
    EXPECT_EQ(callback_count, 1);
}

TEST_F(TaskTransformationTest, TaskTransformationViaTransformator) {
    int callback_count = 0;
    TTask task{
        .events_before_call = {{EEvent::OnTurnEnd, TEventContext{nullptr}}},
        .callback = [&callback_count]() {
            ++callback_count;
            return false;
        }
    };

    TState initial_state = transformator_->CurrentState();

    transformator_->AddTask(scheduler_.get(), std::move(task));

    scheduler_->TriggerEvent({EEvent::OnTurnEnd, TEventContext{nullptr}}, *transformator_);
    EXPECT_EQ(callback_count, 1);

    transformator_->Undo(initial_state);

    scheduler_->TriggerEvent({EEvent::OnTurnEnd, TEventContext{nullptr}}, *transformator_);
    EXPECT_EQ(callback_count, 1);
}

TEST_F(TaskTransformationTest, RemoveTaskWithProgressRestoresCorrectly) {
    int callback_count = 0;
    TTask task{
        .events_before_call = {
            {EEvent::OnTurnStart, TEventContext{nullptr}},
            {EEvent::OnTurnEnd, TEventContext{nullptr}}
        },
        .callback = [&callback_count]() {
            ++callback_count;
            return false;
        }
    };

    TTaskId task_id = scheduler_->AddTaskWithId(std::move(task));

    scheduler_->TriggerEvent({EEvent::OnTurnStart, TEventContext{nullptr}}, *transformator_);
    EXPECT_EQ(callback_count, 0);

    TTask task_copy{
        .events_before_call = {
            {EEvent::OnTurnStart, TEventContext{nullptr}},
            {EEvent::OnTurnEnd, TEventContext{nullptr}}
        },
        .callback = [&callback_count]() {
            ++callback_count;
            return false;
        }
    };

    TRemoveTask remove_task(scheduler_.get(), task_id, std::move(task_copy), 1);

    scheduler_->TriggerEvent({EEvent::OnTurnEnd, TEventContext{nullptr}}, *transformator_);
    EXPECT_EQ(callback_count, 0);

    remove_task.Undo();

    scheduler_->TriggerEvent({EEvent::OnTurnStart, TEventContext{nullptr}}, *transformator_);
    EXPECT_EQ(callback_count, 0);

    scheduler_->TriggerEvent({EEvent::OnTurnEnd, TEventContext{nullptr}}, *transformator_);
    EXPECT_EQ(callback_count, 1);
}

class IntegrationRollbackTest : public ::testing::Test {
protected:
    using FsPath = std::filesystem::path;
    using FsDirEntry = std::filesystem::directory_entry;
    using FsRecursiveIterator = std::filesystem::recursive_directory_iterator;

    void SetUp() override {
        for (const FsDirEntry& dir_entry : FsRecursiveIterator(kPathToData)) {
            if (dir_entry.is_regular_file() && dir_entry.path().extension() == ".json") {
                factory_.AddSource(dir_entry.path());
            }
        }

        auto warrior_id = TGameObjectIdManager::Instance().Register("warrior");
        creature_ = std::make_unique<TCreature>(factory_.Create<TCreature>(warrior_id));
        player_ = std::make_unique<TPlayer>(creature_.get(), TPlayerTeam{0}, TPlayerId{0}, "Test Player", "");
        effect_manager_ = std::make_unique<TEffectManager>();
        scheduler_ = std::make_unique<TTaskScheduler>();
        transformator_ = std::make_unique<TTransformator>(mock_io_);
    }

    TGameObjectFactory factory_;
    TMockInteractionSystem mock_io_;
    std::unique_ptr<TCreature> creature_;
    std::unique_ptr<TPlayer> player_;
    std::unique_ptr<TEffectManager> effect_manager_;
    std::unique_ptr<TTaskScheduler> scheduler_;
    std::unique_ptr<TTransformator> transformator_;
};

TEST_F(IntegrationRollbackTest, FullConditionEffectWithScheduledDecay) {
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 0);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 0);

    TState initial_state = transformator_->CurrentState();

    auto canceler = effect_manager_->AddEffect(TPlayerConditionSet{
        .player = player_.get(),
        .condition = EConditionKind::Frightened,
        .value = 3,
    }, *transformator_);

    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 3);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 3);

    transformator_->AddTask(scheduler_.get(), TTask{
        .events_before_call = {{EEvent::OnTurnStart, TEventContext{player_.get()}}},
        .callback = [canceler]() { return canceler(EEffectCancelPolicy::ReduceUntilZero); },
    });

    scheduler_->TriggerEvent({EEvent::OnTurnStart, TEventContext{player_.get()}}, *transformator_);
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 2);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 2);

    transformator_->Undo(initial_state);

    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 0);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 0);

    scheduler_->TriggerEvent({EEvent::OnTurnStart, TEventContext{player_.get()}}, *transformator_);
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 0);
}

TEST_F(IntegrationRollbackTest, MultipleEffectsAndTasksRollback) {
    TState initial_state = transformator_->CurrentState();

    auto canceler1 = effect_manager_->AddEffect(TPlayerConditionSet{
        .player = player_.get(),
        .condition = EConditionKind::Frightened,
        .value = 2,
    }, *transformator_);

    transformator_->AddTask(scheduler_.get(), TTask{
        .events_before_call = {{EEvent::OnTurnStart, TEventContext{player_.get()}}},
        .callback = [canceler1]() { return canceler1(EEffectCancelPolicy::ReduceUntilZero); },
    });

    auto canceler2 = effect_manager_->AddEffect(TPlayerConditionSet{
        .player = player_.get(),
        .condition = EConditionKind::MultipleAttackPenalty,
        .value = 5,
    }, *transformator_);

    transformator_->AddTask(scheduler_.get(), TTask{
        .events_before_call = {{EEvent::OnTurnEnd, TEventContext{player_.get()}}},
        .callback = [canceler2]() { return canceler2(EEffectCancelPolicy::Cancel); },
    });

    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 2);
    EXPECT_EQ(creature_->Get(EConditionKind::MultipleAttackPenalty), 5);

    transformator_->Undo(initial_state);

    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 0);
    EXPECT_EQ(creature_->Get(EConditionKind::MultipleAttackPenalty), 0);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 0);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::MultipleAttackPenalty), 0);
}

TEST_F(IntegrationRollbackTest, ClearConditionStopsScheduledRevival) {
    auto canceler = effect_manager_->AddEffect(TPlayerConditionSet{
        .player = player_.get(),
        .condition = EConditionKind::Frightened,
        .value = 2,
    }, *transformator_);
    transformator_->AddTask(scheduler_.get(), TTask{
        .events_before_call = {{EEvent::OnTurnStart, TEventContext{player_.get()}}},
        .callback = [canceler]() { return canceler(EEffectCancelPolicy::ReduceUntilZero); },
    });

    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 2);

    effect_manager_->ClearCondition(player_.get(), EConditionKind::Frightened, *transformator_);
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 0);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 0);

    scheduler_->TriggerEvent({EEvent::OnTurnStart, TEventContext{player_.get()}}, *transformator_);
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 0);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), EConditionKind::Frightened), 0);

    scheduler_->TriggerEvent({EEvent::OnTurnStart, TEventContext{player_.get()}}, *transformator_);
    scheduler_->TriggerEvent({EEvent::OnTurnStart, TEventContext{player_.get()}}, *transformator_);
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 0);
}

TEST_F(IntegrationRollbackTest, FrightenedNaturalDecayUnaffected) {
    auto canceler = effect_manager_->AddEffect(TPlayerConditionSet{
        .player = player_.get(),
        .condition = EConditionKind::Frightened,
        .value = 2,
    }, *transformator_);
    transformator_->AddTask(scheduler_.get(), TTask{
        .events_before_call = {{EEvent::OnTurnStart, TEventContext{player_.get()}}},
        .callback = [canceler]() { return canceler(EEffectCancelPolicy::ReduceUntilZero); },
    });

    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 2);
    scheduler_->TriggerEvent({EEvent::OnTurnStart, TEventContext{player_.get()}}, *transformator_);
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 1);
    scheduler_->TriggerEvent({EEvent::OnTurnStart, TEventContext{player_.get()}}, *transformator_);
    EXPECT_EQ(creature_->Get(EConditionKind::Frightened), 0);
}

TEST_F(IntegrationRollbackTest, ClearConditionFallbackForDirectSet) {
    transformator_->ChangeCondition(creature_.get(), EConditionKind::Prone, 1);
    EXPECT_EQ(creature_->Get(EConditionKind::Prone), 1);

    effect_manager_->ClearCondition(player_.get(), EConditionKind::Prone, *transformator_);
    EXPECT_EQ(creature_->Get(EConditionKind::Prone), 0);
}

TEST(SchedulerReentrancy, CallbackAddingTaskDuringTriggerIsSafe) {
    TMockInteractionSystem io;
    TTransformator transformator(io);
    TTaskScheduler scheduler;

    const TEvent turn_end{EEvent::OnTurnEnd, TEventContext{nullptr}};
    const TEvent round_end{EEvent::OnRoundEnd, TEventContext{nullptr}};

    int fired = 0;
    int added_fired = 0;
    transformator.AddTask(&scheduler, TTask{
        .events_before_call = {turn_end},
        .callback = [&]() {
            ++fired;
            transformator.AddTask(&scheduler, TTask{
                .events_before_call = {round_end},
                .callback = [&]() { ++added_fired; return false; },
            });
            return false;
        },
    });

    // Adding a task mid-trigger must not invalidate iteration; the new task
    // waits on a different event and is not processed during this trigger.
    scheduler.TriggerEvent(turn_end, transformator);
    EXPECT_EQ(fired, 1);
    EXPECT_EQ(added_fired, 0);

    // The task added mid-trigger is still scheduled and fires on its own event.
    scheduler.TriggerEvent(round_end, transformator);
    EXPECT_EQ(added_fired, 1);
}

TEST(SchedulerReentrancy, CallbackRemovingAnotherTaskDuringTriggerIsSafe) {
    TMockInteractionSystem io;
    TTransformator transformator(io);
    TTaskScheduler scheduler;

    const TEvent turn_end{EEvent::OnTurnEnd, TEventContext{nullptr}};

    int remover_fired = 0;
    int victim_fired = 0;
    TTaskId victim_id = 0;
    bool have_victim = false;

    // Added first, so it is processed before the victim in id order.
    transformator.AddTask(&scheduler, TTask{
        .events_before_call = {turn_end},
        .callback = [&]() {
            ++remover_fired;
            if (have_victim) {
                transformator.RemoveTask(&scheduler, victim_id,
                    scheduler.GetTaskCopy(victim_id),
                    scheduler.GetTaskProgress(victim_id));
            }
            return false;
        },
    });

    victim_id = transformator.AddTask(&scheduler, TTask{
        .events_before_call = {turn_end},
        .callback = [&]() { ++victim_fired; return false; },
    });
    have_victim = true;

    // The remover deletes the victim before iteration reaches it; must not
    // crash, and the removed task must not fire.
    scheduler.TriggerEvent(turn_end, transformator);
    EXPECT_EQ(remover_fired, 1);
    EXPECT_EQ(victim_fired, 0);
}

TEST(SchedulerReentrancy, EmptyEventListIsRejected) {
    TMockInteractionSystem io;
    TTransformator transformator(io);
    TTaskScheduler scheduler;

    EXPECT_THROW(
        transformator.AddTask(&scheduler, TTask{
            .events_before_call = {},
            .callback = []() { return false; },
        }),
        std::invalid_argument);
}
