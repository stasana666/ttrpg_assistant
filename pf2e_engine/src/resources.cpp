#include <cassert>
#include <resources.h>

bool TResourceId::operator ==(const TResourceId& other) const
{
    return id == other.id;
}

bool TResourceId::operator !=(const TResourceId& other) const
{
    return id == other.id;
}

TResourceId::TResourceId(int id)
    : id(id)
{
}

/////////////////////////////////////////////////////////////////////////

TResourceId TResourceManager::Register(const std::string& name)
{
    auto it = table.find(name);
    if (it == table.end()) {
        table[name] = names.size();
        names.emplace_back(name);
        return TResourceId(table.size() - 1);
    }
    return TResourceId(it->second);
}

std::string_view TResourceManager::Name(TResourceId id) const
{
    return names[id.id];
}

/////////////////////////////////////////////////////////////////////////

bool TResourcePool::HasResource(TResourceId id, int count) const
{
    assert(count > 0);
    auto it = resources.find(id.id);
    return it != resources.end() && it->second >= count;
}

void TResourcePool::Add(TResourceId id, int count)
{
    assert(count > 0);
    resources[id.id] += count;
}

void TResourcePool::Reduce(TResourceId id, int count)
{
    assert(count > 0);
    auto it = resources.find(id.id);
    if (it != resources.end()) {
        it->second -= count;
        assert(it->second >= 0);
    }
}

int TResourcePool::Count(TResourceId id) const
{
    auto it = resources.find(id.id);
    if (it == resources.end()) {
        return 0;
    }
    return it->second;
}
