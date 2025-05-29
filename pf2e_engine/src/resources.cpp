#include <cassert>
#include <resources.h>

bool TResourcePool::HasResource(TResourceId id, int count) const
{
    assert(count > 0);
    auto it = resources.find(id);
    return it != resources.end() && it->second >= count;
}

void TResourcePool::Add(TResourceId id, int count)
{
    assert(count > 0);
    resources[id] += count;
}

void TResourcePool::Reduce(TResourceId id, int count)
{
    assert(count > 0);
    auto it = resources.find(id);
    if (it != resources.end()) {
        it->second -= count;
        assert(it->second >= 0);
    }
}

int TResourcePool::Count(TResourceId id) const
{
    auto it = resources.find(id);
    if (it == resources.end()) {
        return 0;
    }
    return it->second;
}
