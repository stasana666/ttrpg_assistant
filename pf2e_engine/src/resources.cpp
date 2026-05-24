#include <cassert>
#include <resources.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>

#include <algorithm>
#include <utility>
#include <vector>

bool TResourcePool::HasResource(TResourceId id, int count) const
{
    assert(count > 0);
    auto it = resources_.find(id);
    return it != resources_.end() && it->second >= count;
}

void TResourcePool::Add(TResourceId id, int count)
{
    assert(count > 0);
    resources_[id] += count;
}

void TResourcePool::Reduce(TResourceId id, int count)
{
    assert(count >= 0);
    auto it = resources_.find(id);
    if (it != resources_.end()) {
        it->second -= count;
        assert(it->second >= 0);
    }
}

int TResourcePool::Count(TResourceId id) const
{
    auto it = resources_.find(id);
    if (it == resources_.end()) {
        return 0;
    }
    return it->second;
}

TAstNode TResourcePool::GetAst([[maybe_unused]] TAstContext& ctx) const
{
    static constexpr size_t kExpectedSize = 64;
    static constexpr size_t kExpectedSentinelOffset = 56;
    AST_ASSERT_LAYOUT_WITH_SENTINEL(TResourcePool, kExpectedSize, kExpectedSentinelOffset);

    // Sort entries by resource name (stable across runs, unlike the integer id).
    // Filter out 0-counts: TResourcePool::Count(id) returns 0 both for absent
    // keys and zero-mapped keys. TChangeResource::Undo reduces but does not
    // erase, leaving stale {id, 0} entries that would falsely fail rollback
    // equality otherwise. AST represents logical state, not map structure.
    std::vector<std::pair<std::string, int>> sorted;
    sorted.reserve(resources_.size());
    for (const auto& [id, count] : resources_) {
        if (count != 0) {
            sorted.emplace_back(std::string(TResourceIdManager::Instance().Name(id)), count);
        }
    }
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    TAstNode node = TAstNode::MakeObject("TResourcePool");
    for (const auto& [name, count] : sorted) {
        AddValueField(node, name, count);
    }
    return node;
}
