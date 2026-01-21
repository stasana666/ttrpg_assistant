#include <gtest/gtest.h>

#include <pf2e_engine/transformation/transformation.h>
#include <pf2e_engine/transformation/transformator.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/resources.h>
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
    }

    TGameObjectFactory factory_;
    std::unique_ptr<TCreature> creature_;
};

TEST_F(TransformationTest, ChangeConditionAppliesValue) {
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 0);

    TChangeCondition change(creature_.get(), ECondition::Frightened, 3);

    EXPECT_EQ(creature_->Get(ECondition::Frightened), 3);
}

TEST_F(TransformationTest, ChangeConditionUndoRestoresPreviousValue) {
    creature_->Set(ECondition::Frightened, 2);
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 2);

    TChangeCondition change(creature_.get(), ECondition::Frightened, 5);
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 5);

    change.Undo();
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 2);
}

TEST_F(TransformationTest, ChangeConditionUndoFromZero) {
    EXPECT_EQ(creature_->Get(ECondition::MultipleAttackPenalty), 0);

    TChangeCondition change(creature_.get(), ECondition::MultipleAttackPenalty, 5);
    EXPECT_EQ(creature_->Get(ECondition::MultipleAttackPenalty), 5);

    change.Undo();
    EXPECT_EQ(creature_->Get(ECondition::MultipleAttackPenalty), 0);
}

TEST(ChangeResourceTest, AddResourceAppliesValue) {
    TResourcePool pool;
    TResourceId test_id = TResourceIdManager::Instance().Register("test_resource");

    EXPECT_EQ(pool.Count(test_id), 0);

    TChangeResource change(&pool, test_id, 5);

    EXPECT_EQ(pool.Count(test_id), 5);
}

TEST(ChangeResourceTest, AddResourceUndoRemovesValue) {
    TResourcePool pool;
    TResourceId test_id = TResourceIdManager::Instance().Register("test_resource_undo");

    TChangeResource change(&pool, test_id, 5);
    EXPECT_EQ(pool.Count(test_id), 5);

    change.Undo();
    EXPECT_EQ(pool.Count(test_id), 0);
}

TEST(ChangeResourceTest, ReduceResourceAppliesValue) {
    TResourcePool pool;
    TResourceId test_id = TResourceIdManager::Instance().Register("test_resource_reduce");
    pool.Add(test_id, 10);

    EXPECT_EQ(pool.Count(test_id), 10);

    TChangeResource change(&pool, test_id, -3);

    EXPECT_EQ(pool.Count(test_id), 7);
}

TEST(ChangeResourceTest, ReduceResourceUndoRestoresValue) {
    TResourcePool pool;
    TResourceId test_id = TResourceIdManager::Instance().Register("test_resource_reduce_undo");
    pool.Add(test_id, 10);

    TChangeResource change(&pool, test_id, -4);
    EXPECT_EQ(pool.Count(test_id), 6);

    change.Undo();
    EXPECT_EQ(pool.Count(test_id), 10);
}

TEST(ChangeResourceTest, ZeroDeltaDoesNothing) {
    TResourcePool pool;
    TResourceId test_id = TResourceIdManager::Instance().Register("test_resource_zero");
    pool.Add(test_id, 5);

    TChangeResource change(&pool, test_id, 0);
    EXPECT_EQ(pool.Count(test_id), 5);

    change.Undo();
    EXPECT_EQ(pool.Count(test_id), 5);
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
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 0);

    transformator_->ChangeCondition(creature_.get(), ECondition::Frightened, 3);
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 3);
}

TEST_F(TransformatorTest, ChangeConditionUndoViaTransformator) {
    TState initial_state = transformator_->CurrentState();

    transformator_->ChangeCondition(creature_.get(), ECondition::Frightened, 3);
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 3);

    transformator_->Undo(initial_state);
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 0);
}

TEST_F(TransformatorTest, AddResourceViaTransformator) {
    TResourceId test_id = TResourceIdManager::Instance().Register("transformator_add_test");

    EXPECT_EQ(creature_->Resources().Count(test_id), 0);

    transformator_->AddResource(&creature_->Resources(), test_id, 5);
    EXPECT_EQ(creature_->Resources().Count(test_id), 5);
}

TEST_F(TransformatorTest, ReduceResourceViaTransformator) {
    TResourceId test_id = TResourceIdManager::Instance().Register("transformator_reduce_test");
    creature_->Resources().Add(test_id, 10);

    transformator_->ReduceResource(&creature_->Resources(), test_id, 3);
    EXPECT_EQ(creature_->Resources().Count(test_id), 7);
}

TEST_F(TransformatorTest, ResourceUndoViaTransformator) {
    TResourceId test_id = TResourceIdManager::Instance().Register("transformator_undo_test");
    creature_->Resources().Add(test_id, 10);

    TState initial_state = transformator_->CurrentState();

    transformator_->ReduceResource(&creature_->Resources(), test_id, 3);
    EXPECT_EQ(creature_->Resources().Count(test_id), 7);

    transformator_->AddResource(&creature_->Resources(), test_id, 2);
    EXPECT_EQ(creature_->Resources().Count(test_id), 9);

    transformator_->Undo(initial_state);
    EXPECT_EQ(creature_->Resources().Count(test_id), 10);
}

TEST_F(TransformatorTest, MixedTransformationsUndo) {
    TResourceId test_id = TResourceIdManager::Instance().Register("transformator_mixed_test");

    TState initial_state = transformator_->CurrentState();

    transformator_->ChangeCondition(creature_.get(), ECondition::Frightened, 2);
    transformator_->AddResource(&creature_->Resources(), test_id, 5);
    transformator_->ChangeCondition(creature_.get(), ECondition::MultipleAttackPenalty, 5);

    EXPECT_EQ(creature_->Get(ECondition::Frightened), 2);
    EXPECT_EQ(creature_->Resources().Count(test_id), 5);
    EXPECT_EQ(creature_->Get(ECondition::MultipleAttackPenalty), 5);

    transformator_->Undo(initial_state);

    EXPECT_EQ(creature_->Get(ECondition::Frightened), 0);
    EXPECT_EQ(creature_->Resources().Count(test_id), 0);
    EXPECT_EQ(creature_->Get(ECondition::MultipleAttackPenalty), 0);
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
    TAddEffect add(effect_manager_.get(), player_.get(), ECondition::Frightened, 3);

    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 3);
}

TEST_F(EffectTransformationTest, AddEffectUndoRemovesValue) {
    TAddEffect add(effect_manager_.get(), player_.get(), ECondition::Frightened, 3);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 3);

    add.Undo();
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 0);
}

TEST_F(EffectTransformationTest, RemoveEffectErasesValue) {
    effect_manager_->InsertValue(player_.get(), ECondition::Frightened, 3);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 3);

    TRemoveEffect remove(effect_manager_.get(), player_.get(), ECondition::Frightened, 3);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 0);
}

TEST_F(EffectTransformationTest, RemoveEffectUndoRestoresValue) {
    effect_manager_->InsertValue(player_.get(), ECondition::Frightened, 3);

    TRemoveEffect remove(effect_manager_.get(), player_.get(), ECondition::Frightened, 3);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 0);

    remove.Undo();
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 3);
}

TEST_F(EffectTransformationTest, EffectManagerAddEffectWithTransformator) {
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 0);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 0);

    TState initial_state = transformator_->CurrentState();

    effect_manager_->AddEffect(TPlayerConditionSet{
        .player = player_.get(),
        .condition = ECondition::Frightened,
        .value = 3,
    }, *transformator_);

    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 3);
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 3);

    // Undo should restore both effect manager state and creature condition
    transformator_->Undo(initial_state);

    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 0);
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 0);
}

TEST_F(EffectTransformationTest, MultipleEffectsWithUndo) {
    TState initial_state = transformator_->CurrentState();

    // Add first effect with value 2
    effect_manager_->AddEffect(TPlayerConditionSet{
        .player = player_.get(),
        .condition = ECondition::Frightened,
        .value = 2,
    }, *transformator_);

    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 2);
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 2);

    // Add second effect with value 5 (higher)
    effect_manager_->AddEffect(TPlayerConditionSet{
        .player = player_.get(),
        .condition = ECondition::Frightened,
        .value = 5,
    }, *transformator_);

    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 5);
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 5);

    // Undo all - should restore to initial state
    transformator_->Undo(initial_state);

    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 0);
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 0);
}

// Task transformation tests

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

    // Trigger the event - callback should be called
    scheduler_->TriggerEvent({EEvent::OnTurnEnd, TEventContext{nullptr}}, *transformator_);
    EXPECT_EQ(callback_count, 1);

    // Undo should remove the task
    add_task.Undo();

    // Trigger again - callback should not be called since task was removed
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

    // Create a copy for removal
    TTask task_copy{
        .events_before_call = {{EEvent::OnTurnEnd, TEventContext{nullptr}}},
        .callback = [&callback_count]() {
            ++callback_count;
            return false;
        }
    };

    TRemoveTask remove_task(scheduler_.get(), task_id, std::move(task_copy), 0);

    // Trigger the event - callback should not be called since task was removed
    scheduler_->TriggerEvent({EEvent::OnTurnEnd, TEventContext{nullptr}}, *transformator_);
    EXPECT_EQ(callback_count, 0);

    // Undo should restore the task
    remove_task.Undo();

    // Trigger again - callback should be called
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

    // Trigger - callback should be called
    scheduler_->TriggerEvent({EEvent::OnTurnEnd, TEventContext{nullptr}}, *transformator_);
    EXPECT_EQ(callback_count, 1);

    // Undo via transformator
    transformator_->Undo(initial_state);

    // Trigger again - task should be removed, callback not called
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

    // Advance progress by triggering first event
    scheduler_->TriggerEvent({EEvent::OnTurnStart, TEventContext{nullptr}}, *transformator_);
    EXPECT_EQ(callback_count, 0);  // Not yet called, waiting for second event

    // Create a copy with current progress (index 1)
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

    // Trigger second event - task was removed, callback not called
    scheduler_->TriggerEvent({EEvent::OnTurnEnd, TEventContext{nullptr}}, *transformator_);
    EXPECT_EQ(callback_count, 0);

    // Undo - task should be restored with progress at index 1
    remove_task.Undo();

    // Trigger first event again - should not advance since we're at index 1
    scheduler_->TriggerEvent({EEvent::OnTurnStart, TEventContext{nullptr}}, *transformator_);
    EXPECT_EQ(callback_count, 0);

    // Trigger second event - should now call callback
    scheduler_->TriggerEvent({EEvent::OnTurnEnd, TEventContext{nullptr}}, *transformator_);
    EXPECT_EQ(callback_count, 1);
}

// Integration test: Full rollback scenario simulating a condition effect with scheduled decay
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
    // Initial state: no frightened condition
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 0);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 0);

    TState initial_state = transformator_->CurrentState();

    // Add a Frightened 3 effect (simulating a spell or ability)
    auto canceler = effect_manager_->AddEffect(TPlayerConditionSet{
        .player = player_.get(),
        .condition = ECondition::Frightened,
        .value = 3,
    }, *transformator_);

    // Verify effect is applied
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 3);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 3);

    // Schedule a task to decay the effect at turn start (like the real game logic)
    transformator_->AddTask(scheduler_.get(), TTask{
        .events_before_call = {{EEvent::OnTurnStart, TEventContext{player_.get()}}},
        .callback = [canceler]() { return canceler(EEffectCancelPolicy::ReduceUntilZero); },
    });

    // Simulate one turn passing - frightened should reduce to 2
    scheduler_->TriggerEvent({EEvent::OnTurnStart, TEventContext{player_.get()}}, *transformator_);
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 2);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 2);

    // Now rollback everything - should restore to initial state
    transformator_->Undo(initial_state);

    // Verify complete rollback: no effect, no condition, task removed
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 0);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 0);

    // Trigger event again - nothing should happen since task was rolled back
    scheduler_->TriggerEvent({EEvent::OnTurnStart, TEventContext{player_.get()}}, *transformator_);
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 0);
}

TEST_F(IntegrationRollbackTest, MultipleEffectsAndTasksRollback) {
    TState initial_state = transformator_->CurrentState();

    // Add first effect: Frightened 2
    auto canceler1 = effect_manager_->AddEffect(TPlayerConditionSet{
        .player = player_.get(),
        .condition = ECondition::Frightened,
        .value = 2,
    }, *transformator_);

    transformator_->AddTask(scheduler_.get(), TTask{
        .events_before_call = {{EEvent::OnTurnStart, TEventContext{player_.get()}}},
        .callback = [canceler1]() { return canceler1(EEffectCancelPolicy::ReduceUntilZero); },
    });

    // Add second effect: MultipleAttackPenalty 5
    auto canceler2 = effect_manager_->AddEffect(TPlayerConditionSet{
        .player = player_.get(),
        .condition = ECondition::MultipleAttackPenalty,
        .value = 5,
    }, *transformator_);

    transformator_->AddTask(scheduler_.get(), TTask{
        .events_before_call = {{EEvent::OnTurnEnd, TEventContext{player_.get()}}},
        .callback = [canceler2]() { return canceler2(EEffectCancelPolicy::Cancel); },
    });

    // Verify both effects are active
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 2);
    EXPECT_EQ(creature_->Get(ECondition::MultipleAttackPenalty), 5);

    // Rollback everything
    transformator_->Undo(initial_state);

    // Verify all state is restored
    EXPECT_EQ(creature_->Get(ECondition::Frightened), 0);
    EXPECT_EQ(creature_->Get(ECondition::MultipleAttackPenalty), 0);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::Frightened), 0);
    EXPECT_EQ(effect_manager_->GetHighestValue(player_.get(), ECondition::MultipleAttackPenalty), 0);
}
