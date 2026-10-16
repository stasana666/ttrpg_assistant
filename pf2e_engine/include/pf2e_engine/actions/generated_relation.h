#pragma once

#include <pf2e_engine/actions/timed_relation.h>
#include <pf2e_engine/actions/generated_world.h>
#include <pf2e_engine/common/game_time.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

TRelationAddress MakeRelationAddress(const TGeneratedWorldView& world, std::string_view definition,
                                     const TGeneratedEntityView& owner, const TGeneratedEntityView& key);
void ValidateRelationWrite(const std::shared_ptr<TActionContext>& context,
                            const TGeneratedEntityView& owner, const TGeneratedEntityView& key);
void StoreRelationValue(const std::shared_ptr<TActionContext>& context, TRelationAddress address,
                        std::shared_ptr<const IRelationValue> value, std::optional<TGameDuration> duration);
void RemoveRelationValue(const std::shared_ptr<TActionContext>& context, const TRelationAddress& address);

namespace relation_detail {

template <class TTag>
class TValue final : public IRelationValue {
public:
    explicit TValue(typename TTag::TValue value) : value_(std::move(value)) {}
    std::string_view Id() const override { return TTag::kId; }
    std::type_index Type() const override { return typeid(TTag); }
    TAstNode GetAst(TAstContext& context) const override { return TTag::GetAst(value_, context); }
    const typename TTag::TValue& Read() const { return value_; }

private:
    typename TTag::TValue value_;
};

template <class TTag>
const TValue<TTag>* Find(const TGeneratedWorldView& world,
                         const TGeneratedEntityBinding<typename TTag::TOwner>& owner,
                         const TGeneratedEntityBinding<typename TTag::TKey>& key)
{
    const auto address = MakeRelationAddress(world, TTag::kId, owner.View(), key.View());
    world.RelationRules().Require(TTag::kId, typeid(TTag), address.Owner.Type, address.Key.Type);
    const auto* value = world.Relations().Find(address, world.ElapsedSeconds());
    if (value == nullptr) {
        return nullptr;
    }
    const auto* typed = dynamic_cast<const TValue<TTag>*>(value);
    if (typed == nullptr) {
        throw std::invalid_argument("relation payload does not match its declared type");
    }
    return typed;
}

template <class TTag>
void Set(const std::shared_ptr<TActionContext>& context,
          const TGeneratedEntityBinding<typename TTag::TOwner>& owner,
          const TGeneratedEntityBinding<typename TTag::TKey>& key,
          typename TTag::TValue value, std::optional<TGameDuration> duration)
{
    ValidateRelationWrite(context, owner.View(), key.View());
    (void)owner.Guard();
    auto address = MakeRelationAddress(TGeneratedWorldView(context->battle), TTag::kId,
                                        owner.View(), key.View());
    StoreRelationValue(context, std::move(address),
        std::make_shared<const TValue<TTag>>(std::move(value)), duration);
}

}

template <class TTag>
bool HasRelation(const TGeneratedWorldView& world,
                  const TGeneratedEntityBinding<typename TTag::TOwner>& owner,
                  const TGeneratedEntityBinding<typename TTag::TKey>& key)
{
    return relation_detail::Find<TTag>(world, owner, key) != nullptr;
}

template <class TTag>
typename TTag::TValue GetRelation(const TGeneratedWorldView& world,
                                   const TGeneratedEntityBinding<typename TTag::TOwner>& owner,
                                   const TGeneratedEntityBinding<typename TTag::TKey>& key,
                                   typename TTag::TValue fallback)
{
    const auto* found = relation_detail::Find<TTag>(world, owner, key);
    return found == nullptr ? std::move(fallback) : found->Read();
}

template <class TTag>
void SetRelation(const std::shared_ptr<TActionContext>& context,
                  const TGeneratedEntityBinding<typename TTag::TOwner>& owner,
                  const TGeneratedEntityBinding<typename TTag::TKey>& key,
                  typename TTag::TValue value, TGameDuration duration)
{
    relation_detail::Set<TTag>(context, owner, key, std::move(value), duration);
}

template <class TTag>
void SetRelation(const std::shared_ptr<TActionContext>& context,
                  const TGeneratedEntityBinding<typename TTag::TOwner>& owner,
                  const TGeneratedEntityBinding<typename TTag::TKey>& key,
                  typename TTag::TValue value)
{
    relation_detail::Set<TTag>(context, owner, key, std::move(value), std::nullopt);
}

template <class TTag>
void EraseRelation(const std::shared_ptr<TActionContext>& context,
                    const TGeneratedEntityBinding<typename TTag::TOwner>& owner,
                    const TGeneratedEntityBinding<typename TTag::TKey>& key)
{
    ValidateRelationWrite(context, owner.View(), key.View());
    (void)owner.Guard();
    const TGeneratedWorldView world(context->battle);
    const auto address = MakeRelationAddress(world, TTag::kId, owner.View(), key.View());
    world.RelationRules().Require(TTag::kId, typeid(TTag), address.Owner.Type, address.Key.Type);
    RemoveRelationValue(context, address);
}
