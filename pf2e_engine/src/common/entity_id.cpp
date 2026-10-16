#include <pf2e_engine/common/entity_id.h>

#include <algorithm>
#include <limits>
#include <mutex>
#include <stdexcept>

namespace {

struct TEntityIdState {
    std::mutex Mutex;
    std::uint64_t Next = 1;
};

TEntityIdState& EntityIdState()
{
    static TEntityIdState state;
    return state;
}

}

TEntityId NewEntityId()
{
    auto& state = EntityIdState();
    const std::lock_guard lock(state.Mutex);
    return {state.Next++};
}

TEntityId RestoreEntityId(std::uint64_t value)
{
    if (value == 0 || value == std::numeric_limits<std::uint64_t>::max()) {
        throw std::invalid_argument("invalid entity identity");
    }
    auto& state = EntityIdState();
    const std::lock_guard lock(state.Mutex);
    state.Next = std::max(state.Next, value + 1);
    return {value};
}

std::uint64_t PeekNextEntityId()
{
    auto& state = EntityIdState();
    const std::lock_guard lock(state.Mutex);
    return state.Next;
}
