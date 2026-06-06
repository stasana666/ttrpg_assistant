#pragma once

#include <pf2e_engine/actions/save_point.h>

#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <utility>

namespace continuation {

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

template <class TCondition, class TBody>
void While(TCondition condition, TBody body)
{
    if (!condition()) {
        return;
    }
    Then(body, [condition, body]() { While(condition, body); });
}

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

}

template <class TContainer, class TFunc>
void ForEachOwned(TContainer container, TFunc func)
{
    detail::ForEachOwnedAt(
        std::make_shared<TContainer>(std::move(container)),
        std::size_t{0},
        std::move(func));
}

}
