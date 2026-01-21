#include <pf2e_engine/scheduler.h>
#include <pf2e_engine/transformation/transformator.h>

void TTaskScheduler::TriggerEvent(TEvent event, TTransformator& transformator)
{
    // Collect tasks to remove (can't modify list while iterating)
    std::vector<std::tuple<TTaskId, TTask, size_t>> tasks_to_remove;

    for (auto& [id, task, current] : tasks_) {
        size_t current_index = static_cast<size_t>(current - task.events_before_call.begin());
        if (*current == event) {
            transformator.AdvanceTaskProgress(this, id, current_index + 1);
        }
        // Re-read current after transformation may have changed it
        current_index = static_cast<size_t>(current - task.events_before_call.begin());
        if (current_index == task.events_before_call.size()) {
            if (task.callback()) {
                // Callback wants to repeat - reset to beginning
                transformator.AdvanceTaskProgress(this, id, 0);
            } else {
                // Task completed - will be removed
                tasks_to_remove.emplace_back(id, task, task.events_before_call.size());
            }
        }
    }

    // Remove completed tasks via transformations
    for (auto& [id, task, progress] : tasks_to_remove) {
        transformator.RemoveTask(this, id, std::move(task), progress);
    }
}

TTaskId TTaskScheduler::AddTaskWithId(TTask&& task)
{
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
