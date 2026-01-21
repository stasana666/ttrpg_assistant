#pragma once

#include <pf2e_engine/condition.h>
#include <pf2e_engine/mechanics/hitpoints.h>
#include <pf2e_engine/resources.h>
#include <pf2e_engine/scheduler.h>
#include <variant>

class TCreature;
class TEffectManager;
class TInitiativeOrder;
class TPlayer;
class TTaskScheduler;

class TChangeHitPoints {
public:
    TChangeHitPoints(THitPoints* hitpoints, int value);

    void Undo();

private:
    THitPoints* hitpoints_;
    THitPoints prev_;
};

class TChangeCondition {
public:
    TChangeCondition(TCreature* creature, ECondition condition, int new_value);

    void Undo();

private:
    TCreature* creature_;
    ECondition condition_;
    int prev_value_;
};

class TChangeResource {
public:
    TChangeResource(TResourcePool* pool, TResourceId id, int delta);

    void Undo();

private:
    TResourcePool* pool_;
    TResourceId id_;
    int delta_;  // positive = added, negative = reduced
};

class TAddEffect {
public:
    TAddEffect(TEffectManager* manager, TPlayer* player, ECondition condition, int value);

    void Undo();

private:
    TEffectManager* manager_;
    TPlayer* player_;
    ECondition condition_;
    int value_;
};

class TRemoveEffect {
public:
    TRemoveEffect(TEffectManager* manager, TPlayer* player, ECondition condition, int value);

    void Undo();

private:
    TEffectManager* manager_;
    TPlayer* player_;
    ECondition condition_;
    int value_;
};

class TAddTask {
public:
    TAddTask(TTaskScheduler* scheduler, TTask task);

    void Undo();

    TTaskId GetTaskId() const { return task_id_; }

private:
    TTaskScheduler* scheduler_;
    TTaskId task_id_;
};

class TRemoveTask {
public:
    TRemoveTask(TTaskScheduler* scheduler, TTaskId id, TTask task, size_t progress_index);

    void Undo();

private:
    TTaskScheduler* scheduler_;
    TTaskId task_id_;
    TTask task_;
    size_t progress_index_;
};

class TAdvanceTaskProgress {
public:
    TAdvanceTaskProgress(TTaskScheduler* scheduler, TTaskId id, size_t new_index);

    void Undo();

private:
    TTaskScheduler* scheduler_;
    TTaskId task_id_;
    size_t prev_index_;
};

class TChangeCurrentPlayer {
public:
    TChangeCurrentPlayer(TInitiativeOrder* order, size_t new_position);

    void Undo();

private:
    TInitiativeOrder* order_;
    size_t prev_position_;
};

class TChangeRound {
public:
    TChangeRound(TInitiativeOrder* order, size_t new_round);

    void Undo();

private:
    TInitiativeOrder* order_;
    size_t prev_round_;
};

using TTransformation = std::variant<
    TChangeHitPoints,
    TChangeCondition,
    TChangeResource,
    TAddEffect,
    TRemoveEffect,
    TAddTask,
    TRemoveTask,
    TAdvanceTaskProgress,
    TChangeCurrentPlayer,
    TChangeRound
>;
