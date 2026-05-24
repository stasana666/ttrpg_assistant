#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

// Threaded through every GetAst(ctx) call. Owns:
//   - the Visited set used for ownership-cycle detection;
//   - an identity table mapping non-owning pointers (e.g. TPlayer*) to
//     stable string IDs (e.g. "player#3") so references can serialize
//     deterministically without exposing addresses.
//
// Cycle detection applies ONLY to ownership traversal. Non-owning references
// do not participate (AddReference does not call Visit).
//
// The visited set is keyed on (address, type) — NOT address alone — because
// a child member often shares its parent's address (e.g. the first array
// element of a class member is at the same byte offset as the class itself,
// and TCharacteristicSet's `stats_[0]` lives at offsetof(stats_) == 0).
// Without the type key, a parent and its first-member child would falsely
// register as a cycle on every traversal.
class TAstContext {
public:
    // Throws std::runtime_error if (p, type) was already visited.
    void Visit(const void* p, std::string_view type);
    void Unvisit(const void* p, std::string_view type);

    void RegisterIdentity(const void* p, std::string id);
    std::string IdentityOf(const void* p) const;  // "" if unknown

    // RAII guard: visits on construction, unvisits on destruction.
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
};
