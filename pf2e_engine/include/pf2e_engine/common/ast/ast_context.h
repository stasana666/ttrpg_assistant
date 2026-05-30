#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class TAstNode;

class TAstContext {
public:
    void Visit(const void* p, std::string_view type);
    void Unvisit(const void* p, std::string_view type);

    void RegisterIdentity(const void* p, std::string id);
    std::string IdentityOf(const void* p) const;

    void RegisterPending(const void* p, TAstNode* node);

    class TGuard {
    public:
        TGuard(TAstContext& ctx, const void* p, std::string_view type);
        ~TGuard();

        TGuard(const TGuard&) = delete;
        TGuard& operator =(const TGuard&) = delete;

    private:
        TAstContext& ctx_;
        const void* p_;
        std::string type_;
    };

private:
    using TKey = std::pair<const void*, std::string>;
    struct FKeyHash {
        size_t operator()(const TKey& k) const noexcept {
            return std::hash<const void*>()(k.first) ^
                   (std::hash<std::string>()(k.second) << 1);
        }
    };

    std::unordered_set<TKey, FKeyHash> visited_;
    std::unordered_map<const void*, std::string> identity_;
    std::unordered_map<const void*, std::vector<TAstNode*>> pending_;
};
