#include <pf2e_engine/actions/generated_relation.h>

#include <pf2e_engine/battle.h>

TRelationAddress MakeRelationAddress(const TGeneratedWorldView& world, std::string_view definition,
                                     const TGeneratedEntityView& owner, const TGeneratedEntityView& key)
{
    const auto capture = [&world](const TGeneratedEntityView& entity) {
        world.Validate(entity);
        return TRecordedEntityKey{entity.Player().GetCreature()->Id(), entity.Player().GetId(),
                                   &entity.Player(), entity.Player().GetCreature(), entity.Type()};
    };
    return {std::string(definition), capture(owner), capture(key)};
}

void ValidateRelationWrite(const std::shared_ptr<TActionContext>& context,
                            const TGeneratedEntityView& owner, const TGeneratedEntityView& key)
{
    ValidateActionActor(context);
    ValidateActionEntityMembership(context, owner);
    ValidateActionEntityMembership(context, key);
}

void StoreRelationValue(const std::shared_ptr<TActionContext>& context, TRelationAddress address,
                        std::shared_ptr<const IRelationValue> value, std::optional<TGameDuration> duration)
{
    if (!context || context->battle == nullptr || context->transformator == nullptr) {
        throw std::invalid_argument("relation write requires a battle and journal");
    }
    context->battle->SetTimedRelation(std::move(address), std::move(value), duration, *context->transformator);
}

void RemoveRelationValue(const std::shared_ptr<TActionContext>& context, const TRelationAddress& address)
{
    if (!context || context->battle == nullptr || context->transformator == nullptr) {
        throw std::invalid_argument("relation removal requires a battle and journal");
    }
    context->battle->EraseTimedRelation(address, *context->transformator);
}
