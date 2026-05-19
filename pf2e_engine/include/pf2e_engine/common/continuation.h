#pragma once

#include <pf2e_engine/actions/save_point.h>

#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <utility>

// Continuation-aware execution helpers.
//
// Gameplay code occasionally needs to *suspend*: a step throws
// TSavepointStackUnwind, the stack unwinds, and the work can be Resume()d later
// from exactly where it stopped. For that to work, every frame between the
// suspension point and the top-level handler must record how to finish the rest
// of its own work once the savepoint resumes.
//
// These helpers centralize that bookkeeping. They know nothing about battles,
// creatures or actions -- only about propagating continuations correctly across
// the call stack. They are exception-correct and support arbitrary depth.
namespace continuation {

// Runs `step`. If `step` suspends (throws TSavepointStackUnwind), `tail` is
// scheduled to run when the savepoint is resumed; otherwise `tail` runs now.
//
// Then is intentionally recursive: the scheduled continuation re-enters Then
// rather than plainly running `resume(); tail();`. That way, if `resume()`
// itself suspends again, `tail` is re-protected onto the new savepoint instead
// of being silently dropped.
inline void Then(std::function<void()> step, std::function<void()> tail)
{
    try {
        step();
    } catch (TSavepointStackUnwind& save_point) {
        save_point.AddCallFunctionLevel([tail](TSavepointCallback resume) {
            Then(std::move(resume), tail);
        });
        throw;
    }
    tail();
}

// Continuation-aware while-loop: repeats `body` while `condition` holds.
// If `body` suspends, the loop resumes with the remaining iterations.
template <class TCondition, class TBody>
void While(TCondition condition, TBody body)
{
    if (!condition()) {
        return;
    }
    Then(body, [condition, body]() { While(condition, body); });
}

// Continuation-aware for_each over the iterator range [begin, end).
// The range must outlive any suspension -- use it for stable storage such as a
// member container; for a temporary container use ForEachOwned instead.
template <class TIterator, class TFunc>
void ForEach(TIterator begin, TIterator end, TFunc func)
{
    if (begin == end) {
        return;
    }
    Then(
        [=]() { func(*begin); },
        [=]() { ForEach(std::next(begin), end, func); });
}

namespace detail {

template <class TContainer, class TFunc>
void ForEachOwnedAt(std::shared_ptr<TContainer> data, std::size_t index, TFunc func)
{
    if (index >= data->size()) {
        return;
    }
    Then(
        [=]() { func((*data)[index]); },
        [=]() { ForEachOwnedAt(data, index + 1, func); });
}

}  // namespace detail

// Continuation-aware for_each that takes ownership of `container`. The elements
// are moved onto the heap (shared_ptr) and that handle is captured into every
// continuation level, so they stay alive across stack unwinding -- unlike a
// plain iterator range over a local container, whose storage would dangle.
template <class TContainer, class TFunc>
void ForEachOwned(TContainer container, TFunc func)
{
    detail::ForEachOwnedAt(
        std::make_shared<TContainer>(std::move(container)),
        std::size_t{0},
        std::move(func));
}

}  // namespace continuation
