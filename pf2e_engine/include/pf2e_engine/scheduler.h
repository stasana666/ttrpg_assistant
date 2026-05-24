#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/player.h>

#include <list>

enum class EEvent {
    OnRoundStart,
    OnRoundEnd,
    OnTurnStart,
    OnTurnEnd,
};

struct TEventContext {
    TPlayer* player;

    bool operator ==(const TEventContext& other) const = default;
};

struct TEvent {
    EEvent type;
    TEventContext context;

    bool operator ==(const TEvent& other) const = default;
};

struct TTask {
    std::vector<TEvent> events_before_call;
    std::function<bool()> callback;
};

using TTaskId = size_t;

class TTransformator;

class TTaskScheduler {
public:
    void TriggerEvent(TEvent event, TTransformator& transformator);

    // Returns task ID for transformation tracking
    TTaskId AddTaskWithId(TTask&& task);

    // For transformation undo access
    void RemoveTaskById(TTaskId id);
    void RestoreTask(TTaskId id, TTask task, size_t progress_index);
    void SetTaskProgress(TTaskId id, size_t progress_index);
    size_t GetTaskProgress(TTaskId id) const;
    TTask GetTaskCopy(TTaskId id) const;

    TAstNode GetAst(TAstContext& ctx) const;

private:
    TTaskId next_task_id_ = 0;
    std::list<std::tuple<TTaskId, TTask, std::vector<TEvent>::iterator>> tasks_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TTaskScheduler> : std::true_type {};

// Free helpers (TEvent / TTask are POD-ish structs without a class to host
// a method on cleanly; offering them as free functions keeps the per-class
// noise down).
TAstNode GetEventAst(const TEvent& event, TAstContext& ctx);
TAstNode GetTaskAst(const TTask& task, TAstContext& ctx, size_t progress_index);
