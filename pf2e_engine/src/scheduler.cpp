#include <pf2e_engine/scheduler.h>

void TTaskScheduler::TriggerEvent(TEvent event)
{
    for (auto& task : tasks_) {
        if (task.events_before_call.back() == event) {
            task.events_before_call.pop_back();
        }
        if (task.events_before_call.empty()) {
            task.callback();
        }
    }
    auto new_end = std::remove_if(tasks_.begin(), tasks_.end(), [](const TTask& task) {
        return task.events_before_call.empty();
    });
    tasks_.erase(new_end, tasks_.end());
}

void TTaskScheduler::AddTask(TTask&& task)
{
    std::reverse(task.events_before_call.begin(), task.events_before_call.end());
    tasks_.emplace_back(std::move(task));
}
