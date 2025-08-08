#pragma once

#include <pf2e_engine/common/value_id.h>
#include <unordered_map>

struct TResourceTag {};

using TResourceId = TValueId<TResourceTag>;
using TResourceIdManager = TValueIdManager<TResourceTag>;
using TResourceIdHash = TValueIdHash<TResourceTag>;

class TResourcePool {
public:
    bool HasResource(TResourceId id, int count) const;
    void Reduce(TResourceId id, int count);
    void Add(TResourceId id, int count);
    int Count(TResourceId id) const;

private:
    std::unordered_map<TResourceId, int, TResourceIdHash> resources_;
};
