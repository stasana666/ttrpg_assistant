#include <pf2e_engine/scheduler.h>

void TTaskScheduler::TriggerEvent(TEvent event)
{
    for (auto& [task, current] : tasks_) {
        if (*current == event) {
            ++current;
        }
        if (current == task.events_before_call.end()) {
            if (task.callback()) {
                current = task.events_before_call.begin();
            }
        }
    }
    auto new_end = std::remove_if(tasks_.begin(), tasks_.end(), [](const auto& task) {
        return task.second == task.first.events_before_call.end();
    });
    tasks_.erase(new_end, tasks_.end());
}

void TTaskScheduler::AddTask(TTask&& task)
{
    tasks_.emplace_back(std::move(task), task.events_before_call.begin());
    tasks_.back().second = tasks_.back().first.events_before_call.begin();
}
