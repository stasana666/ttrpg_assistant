#pragma once

#include <unordered_map>
#include <string_view>
#include <vector>

class TResourceId {
public:
    bool operator ==(const TResourceId& other) const;
    bool operator !=(const TResourceId& other) const;

private:
    friend class TResourceManager;
    friend class TResourcePool;

    TResourceId(int id);

    int id;
};

class TResourceManager {
public:
    static TResourceManager& instance() {
        static TResourceManager instance;
        return instance;
    }

    // TODO: std::string -> std::string_view, https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0919r2.html
    TResourceId Register(const std::string& name);
    std::string_view Name(TResourceId id) const;

private:

    std::unordered_map<std::string, int> table;
    std::vector<std::string> names;
};

class TResourcePool {
public:
    bool HasResource(TResourceId id, int count) const;
    void Reduce(TResourceId id, int count);
    void Add(TResourceId id, int count);
    int Count(TResourceId id) const;

private:
    std::unordered_map<int, int> resources;
};
