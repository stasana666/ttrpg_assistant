#include <pf2e_engine/actions/timed_relation.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>
#include <pf2e_engine/transformation/transformator.h>

#include <stdexcept>
#include <utility>

namespace {

bool SameEntity(const TRecordedEntityKey& first, const TRecordedEntityKey& second)
{
    return first.Id == second.Id && first.PlayerId == second.PlayerId
        && first.Identity == second.Identity && first.Address == second.Address && first.Type == second.Type;
}

bool SameAddress(const TRelationAddress& first, const TRelationAddress& second)
{
    return first.Definition == second.Definition && SameEntity(first.Owner, second.Owner)
        && SameEntity(first.Key, second.Key);
}

}

void TRelationRegistry::RegisterType(std::string id, std::type_index type,
                                    std::type_index owner, std::type_index key)
{
    if (id.empty() || type == typeid(void) || owner == typeid(void) || key == typeid(void)) {
        throw std::invalid_argument("relation requires an identity and type");
    }
    const TType descriptor{type, owner, key};
    const auto [found, inserted] = types_.emplace(std::move(id), descriptor);
    if (!inserted && found->second != descriptor) {
        throw std::invalid_argument("conflicting relation type");
    }
}

void TRelationRegistry::Require(std::string_view id, std::type_index type,
                               std::type_index owner, std::type_index key) const
{
    const auto found = types_.find(id);
    if (found == types_.end() || found->second != TType{type, owner, key}) {
        throw std::invalid_argument("missing or mismatched relation type");
    }
}

TTimedRelationStore::TIndex TTimedRelationStore::Index(const TRelationAddress& address)
{
    return {address.Definition, address.Owner.PlayerId, address.Key.PlayerId};
}

const IRelationValue* TTimedRelationStore::Find(const TRelationAddress& address, std::uint64_t now) const
{
    const auto found = state_.Entries.find(Index(address));
    if (found == state_.Entries.end() || !SameAddress(found->second.Address, address)
        || (found->second.ExpiresAt && now >= *found->second.ExpiresAt)) {
        return nullptr;
    }
    return found->second.Value.get();
}

void TTimedRelationStore::Set(TRelationAddress address, std::optional<std::uint64_t> expires_at,
                              std::shared_ptr<const IRelationValue> value, TTransformator& journal)
{
    if (address.Definition.empty() || !value || value->Id() != address.Definition) {
        throw std::invalid_argument("relation value does not match its declaration");
    }
    auto index = Index(address);
    auto next = state_;
    next.Entries.insert_or_assign(std::move(index), TEntry{std::move(address), expires_at, std::move(value)});
    auto context = journal.ExecutionContext();
    context.Assign(state_, std::move(next), this, "state");
}

void TTimedRelationStore::Erase(const TRelationAddress& address, TTransformator& journal)
{
    const auto index = Index(address);
    const auto found = state_.Entries.find(index);
    if (found == state_.Entries.end() || !SameAddress(found->second.Address, address)) {
        return;
    }
    auto next = state_;
    next.Entries.erase(index);
    auto context = journal.ExecutionContext();
    context.Assign(state_, std::move(next), this, "state");
}

TAstNode TTimedRelationStore::TStateData::GetAst(TAstContext& context) const
{
    AST_ASSERT_LAYOUT(TStateData, 48);
    auto node = TAstNode::MakeObject("TTimedRelationState");
    std::size_t index = 0;
    for (const auto& [key, entry] : Entries) {
        auto value = TAstNode::MakeObject("entry");
        AddValueField(value, "definition", entry.Address.Definition);
        value.AddChild("owner", entry.Address.Owner.GetAst(context));
        value.AddChild("key", entry.Address.Key.GetAst(context));
        if (entry.ExpiresAt) {
            AddValueField(value, "expires_at", *entry.ExpiresAt);
        } else {
            value.AddChild("expires_at", TAstNode::MakeNull());
        }
        value.AddChild("value", entry.Value->GetAst(context));
        node.AddChild(std::to_string(index++), std::move(value));
    }
    return node;
}

TAstNode TTimedRelationStore::GetAst(TAstContext& context) const
{
    AST_ASSERT_LAYOUT(TTimedRelationStore, 56);
    return state_.GetAst(context);
}
