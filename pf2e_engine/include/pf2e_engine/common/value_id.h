#pragma once

#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>

template <typename tag>
class TValueId {
public:
    bool operator ==(const TValueId& other) const;
    bool operator !=(const TValueId& other) const;

private:
    template <typename>
    friend class TValueManager;

    template <typename>
    friend class TValueHash;

    explicit TValueId(int id);

    int id;
};

template <typename tag>
bool TValueId<tag>::operator ==(const TValueId& other) const
{
    return id == other.id;
}

template <typename tag>
bool TValueId<tag>::operator !=(const TValueId& other) const
{
    return id != other.id;
}

template <typename tag>
TValueId<tag>::TValueId(int id)
    : id(id)
{
}

template <typename tag>
class TValueHash {
public:
    std::size_t operator()(const TValueId<tag>& valueId) const noexcept {
        return std::hash<int>()(valueId.id);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////

template <typename tag>
class TValueManager {
public:
    TValueManager() = default;

    static TValueManager& instance() {
        static TValueManager instance;
        return instance;
    }

    TValueId<tag> Register(const std::string& name);
    std::string_view Name(TValueId<tag> id) const;

private:

    std::unordered_map<std::string, int> table;
    std::vector<std::string> names;
};

template <typename tag>
TValueId<tag> TValueManager<tag>::Register(const std::string& name)
{
    auto it = table.find(name);
    if (it == table.end()) {
        table[name] = names.size();
        names.emplace_back(name);
        return TValueId<tag>(table.size() - 1);
    }
    return TValueId<tag>(it->second);
}

template <typename tag>
std::string_view TValueManager<tag>::Name(TValueId<tag> id) const
{
    return names[id.id];
}
