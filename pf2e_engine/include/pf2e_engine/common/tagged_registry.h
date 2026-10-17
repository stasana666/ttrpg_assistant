#pragma once

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <utility>

class TTaggedRegistry {
public:
    template <class TTag, class TValue>
    void Register(std::string id, std::shared_ptr<const TValue> value)
    {
        const std::string tag(TTag::kId);
        if (tag.empty() || id.empty() || !value) {
            throw std::invalid_argument("tagged registration requires identifiers and a value");
        }
        auto& entries = tags_.try_emplace(tag, TEntries{typeid(TTag), {}}).first->second;
        if (entries.Type != typeid(TTag)) {
            throw std::invalid_argument("tag identifier has a different type: " + tag);
        }
        const auto [existing, added] = entries.Values.try_emplace(std::move(id), value);
        if (!added && existing->second != value) {
            throw std::invalid_argument("conflicting registration for " + tag + ": " + existing->first);
        }
    }

    template <class TTag, class TValue>
    std::shared_ptr<const TValue> Find(std::string_view id) const
    {
        const auto entries = tags_.find(TTag::kId);
        if (entries == tags_.end()) {
            return {};
        }
        if (entries->second.Type != typeid(TTag)) {
            throw std::invalid_argument("tag identifier has a different type: " + entries->first);
        }
        const auto value = entries->second.Values.find(id);
        return value == entries->second.Values.end() ? nullptr : std::static_pointer_cast<const TValue>(value->second);
    }

    bool Contains(std::string_view id) const
    {
        for (const auto& [tag, entries] : tags_) {
            if (entries.Values.contains(id)) {
                return true;
            }
        }
        return false;
    }

private:
    struct TEntries {
        std::type_index Type;
        std::map<std::string, std::shared_ptr<const void>, std::less<>> Values;
    };

    std::map<std::string, TEntries, std::less<>> tags_;
};
