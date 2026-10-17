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

std::uint64_t PeekNextEntityId()
{
    auto& state = EntityIdState();
    const std::lock_guard lock(state.Mutex);
    return state.Next;
}
