#pragma once

#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>

template <typename Tag>
class TValueId {
public:
    bool operator ==(const TValueId& other) const;
    bool operator !=(const TValueId& other) const;

private:
    template <typename>
    friend class TValueIdManager;

    template <typename>
    friend class TValueIdHash;

    explicit TValueId(int id);

    int id_;
};

template <typename Tag>
bool TValueId<Tag>::operator ==(const TValueId& other) const
{
    return id_ == other.id_;
}

template <typename Tag>
bool TValueId<Tag>::operator !=(const TValueId& other) const
{
    return id_ != other.id_;
}

template <typename Tag>
TValueId<Tag>::TValueId(int id)
    : id_(id)
{
}

template <typename Tag>
class TValueIdHash {
public:
    std::size_t operator()(const TValueId<Tag>& value_id) const noexcept {
        return std::hash<int>()(value_id.id_);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////

template <typename Tag>
class TValueIdManager {
public:
    TValueIdManager() = default;

    static TValueIdManager& Instance() {
        static TValueIdManager instance;
        return instance;
    }

    TValueId<Tag> Register(const std::string& name);
    std::string_view Name(TValueId<Tag> id) const;

private:

    std::unordered_map<std::string, int> table_;
    std::vector<std::string> names_;
};

template <typename Tag>
TValueId<Tag> TValueIdManager<Tag>::Register(const std::string& name)
{
    auto it = table_.find(name);
    if (it == table_.end()) {
        table_[name] = names_.size();
        names_.emplace_back(name);
        return TValueId<Tag>(table_.size() - 1);
    }
    return TValueId<Tag>(it->second);
}

template <typename Tag>
std::string_view TValueIdManager<Tag>::Name(TValueId<Tag> id) const
{
    return names_[id.id_];
}
