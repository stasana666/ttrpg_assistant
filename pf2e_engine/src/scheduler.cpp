#include <pf2e_engine/scheduler.h>
#include <pf2e_engine/transformation/transformator.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>

#include <algorithm>
#include <stdexcept>

void TTaskScheduler::TriggerEvent(TEvent event, TTransformator& transformator)
{
    auto find_task = [this](TTaskId id) {
        return std::find_if(tasks_.begin(), tasks_.end(),
            [id](const auto& entry) { return std::get<0>(entry) == id; });
    };

    // Snapshot ids up front: a callback may add or remove tasks (through the
    // transformator) during this trigger, so we must not iterate the live list.
    // Every access re-finds the task by id and skips it if a callback removed it.
    std::vector<TTaskId> ids;
    ids.reserve(tasks_.size());
    for (const auto& entry : tasks_) {
        ids.push_back(std::get<0>(entry));
    }

    for (TTaskId id : ids) {
        auto it = find_task(id);
        if (it == tasks_.end()) {
            continue;
        }

        {
            auto& [task_id, task, current] = *it;
            if (current != task.events_before_call.end() && *current == event) {
                size_t index = static_cast<size_t>(current - task.events_before_call.begin());
                transformator.AdvanceTaskProgress(this, id, index + 1);
            }
        }

        it = find_task(id);
        if (it == tasks_.end()) {
            continue;
        }

        size_t event_count = std::get<1>(*it).events_before_call.size();
        size_t progress = static_cast<size_t>(
            std::get<2>(*it) - std::get<1>(*it).events_before_call.begin());
        if (progress != event_count) {
            continue;
        }

        // Copy the task before running the callback: a re-entrant callback may
        // remove this task, which would dangle the reference we still need for
        // AdvanceTaskProgress / RemoveTask below.
        TTask task_copy = std::get<1>(*it);
        if (task_copy.callback()) {
            if (find_task(id) != tasks_.end()) {
                transformator.AdvanceTaskProgress(this, id, 0);
            }
        } else if (find_task(id) != tasks_.end()) {
            transformator.RemoveTask(this, id, std::move(task_copy), event_count);
        }
    }
}

TTaskId TTaskScheduler::AddTaskWithId(TTask&& task)
{
    if (task.events_before_call.empty()) {
        throw std::invalid_argument(
            "TTaskScheduler: task must have at least one event before call");
    }
    TTaskId id = next_task_id_++;
    tasks_.emplace_back(id, std::move(task), std::vector<TEvent>::iterator{});
    std::get<2>(tasks_.back()) = std::get<1>(tasks_.back()).events_before_call.begin();
    return id;
}

void TTaskScheduler::RemoveTaskById(TTaskId id)
{
    tasks_.remove_if([id](const auto& entry) {
        return std::get<0>(entry) == id;
    });
}

void TTaskScheduler::RestoreTask(TTaskId id, TTask task, size_t progress_index)
{
    tasks_.emplace_back(id, std::move(task), std::vector<TEvent>::iterator{});
    auto& [stored_id, stored_task, current] = tasks_.back();
    current = stored_task.events_before_call.begin() + static_cast<std::ptrdiff_t>(progress_index);
}

void TTaskScheduler::SetTaskProgress(TTaskId id, size_t progress_index)
{
    for (auto& [task_id, task, current] : tasks_) {
        if (task_id == id) {
            current = task.events_before_call.begin() + static_cast<std::ptrdiff_t>(progress_index);
            return;
        }
    }
}

size_t TTaskScheduler::GetTaskProgress(TTaskId id) const
{
    for (const auto& [task_id, task, current] : tasks_) {
        if (task_id == id) {
            return static_cast<size_t>(current - task.events_before_call.begin());
        }
    }
    return 0;
}

TTask TTaskScheduler::GetTaskCopy(TTaskId id) const
{
    for (const auto& [task_id, task, current] : tasks_) {
        if (task_id == id) {
            return task;
        }
    }
    return TTask{};
}

TAstNode GetEventAst(const TEvent& event, TAstContext& ctx)
{
    TAstNode node = TAstNode::MakeObject("TEvent");
    AddValueField(node, "type", event.type);
    AddReference(node, "player", event.context.player, ctx);
    return node;
}

TAstNode GetTaskAst(const TTask& task, TAstContext& ctx, size_t progress_index)
{
    TAstNode node = TAstNode::MakeObject("TTask");
    AddValueField(node, "progress_index", progress_index);

    TAstNode events_node = TAstNode::MakeObject("events_before_call");
    for (size_t i = 0; i < task.events_before_call.size(); ++i) {
        events_node.AddChild(std::to_string(i),
            GetEventAst(task.events_before_call[i], ctx));
    }
    node.AddChild("events_before_call", std::move(events_node));

    AddCallbackPlaceholder(node, "callback", task.callback);
    return node;
}

TAstNode TTaskScheduler::GetAst(TAstContext& ctx) const
{
    static constexpr size_t kExpectedSize = 40;
    AST_ASSERT_LAYOUT(TTaskScheduler, kExpectedSize);

    TAstNode node = TAstNode::MakeObject("TTaskScheduler");
    AddValueField(node, "next_task_id", next_task_id_);

    TAstNode tasks_node = TAstNode::MakeObject("tasks");
    size_t idx = 0;
    for (const auto& [id, task, current] : tasks_) {
        size_t progress_index = static_cast<size_t>(
            current - task.events_before_call.begin());
        TAstNode entry = TAstNode::MakeObject("entry");
        AddValueField(entry, "id", id);
        entry.AddChild("task", GetTaskAst(task, ctx, progress_index));
        tasks_node.AddChild(std::to_string(idx++), std::move(entry));
    }
    node.AddChild("tasks", std::move(tasks_node));
    return node;
}
