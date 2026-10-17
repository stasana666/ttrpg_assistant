#include <pf2e_engine/actions/generated_history.h>

#include <pf2e_engine/battle.h>
#include <pf2e_engine/player.h>

TRecordedEntityKey CaptureRecordedEntityKey(const std::shared_ptr<TActionContext>& context,
                                            const TGeneratedEntityView& entity)
{
    ValidateActionEntity(context, entity);
    const auto& player = entity.Player();
    const auto* creature = player.GetCreature();
    return {creature->Id(), player.GetId(), &player, creature, entity.Type()};
}

TRecordedEntityKey CaptureRecordedSourceOwner(const std::shared_ptr<TActionContext>& context,
                                              const TGeneratedSourceKey& source)
{
    ValidateActionActor(context);
    if (context->battle == nullptr) {
        throw TAbilityUnavailable();
    }
    const auto actor = GeneratedActionSelf(context);
    if (source.OwnerIdentity != &actor.Player()) {
        throw TAbilityUnavailable();
    }
    const auto* player = context->battle->GetPlayer([&source](const TPlayer* current) {
        return current == source.OwnerIdentity;
    });
    if (player == nullptr || player->GetCreature()->Id() != source.OwnerId) {
        throw TAbilityUnavailable();
    }
    return CaptureRecordedEntityKey(context, BindGeneratedActionEntity(*player, context->battle));
}

TGeneratedEntityView ResolveRecordedEntity(const TGeneratedWorldView& world,
                                            const TRecordedEntityKey& entity)
{
    return world.ResolveRecordEntity(entity);
}

TGeneratedEntityView ResolveRecordedEntity(const std::shared_ptr<TActionContext>& context,
                                            const TRecordedEntityKey& entity)
{
    ValidateActionActor(context);
    const auto view = ResolveRecordedEntity(TGeneratedWorldView(context->battle), entity);
    return MutableSelectedActionEntity(context, view);
}

const TActionHistory* FindActionHistory(const TGeneratedWorldView& world)
{
    return world.History();
}

std::optional<TActionRecordStamp> FindActionRecordStamp(const TGeneratedWorldView& world,
                                                       const TPlayer& actor, std::string_view id)
{
    const auto* history = FindActionHistory(world);
    if (history == nullptr) {
        return std::nullopt;
    }
    world.Validate(BindGeneratedActionEntity(actor));
    return history->Find(actor, id);
}

bool MatchActionRecordStamp(const TGeneratedWorldView& world, const TPlayer& actor,
                            const TActionRecordStamp& stamp, std::string_view id)
{
    const auto* history = FindActionHistory(world);
    if (history == nullptr) {
        return false;
    }
    world.Validate(BindGeneratedActionEntity(actor));
    return history->Matches(actor, stamp, id);
}

TActionTicket CaptureActionTicket(const std::shared_ptr<TActionContext>& context)
{
    if (!context) {
        throw std::invalid_argument("action ticket requires a context");
    }
    return context->action_ticket;
}

void AcceptActionExecution(const std::shared_ptr<TActionContext>& context, TPlayer& actor,
                            EActionHistoryKind kind)
{
    if (!context || context->transformator == nullptr) {
        throw std::invalid_argument("action acceptance requires a context and journal");
    }
    context->action_ticket = context->battle == nullptr ? TActionTicket{}
        : context->battle->AcceptActionHistory(actor, kind, context->action_definition,
                                               *context->transformator);
}

bool CurrentActionTicket(const std::shared_ptr<TActionContext>& context, const TActionTicket& ticket)
{
    if (!context) {
        throw std::invalid_argument("action publication requires a context");
    }
    if (context->battle == nullptr || !ticket) {
        return false;
    }
    const auto actor = GeneratedActionSelf(context);
    return context->battle->History().Current(actor.Player(), ticket);
}

void StoreActionRecord(const std::shared_ptr<TActionContext>& context, const TActionTicket& ticket,
                       std::shared_ptr<const IActionRecordValue> value)
{
    if (!CurrentActionTicket(context, ticket)) {
        return;
    }
    const auto actor = GeneratedActionSelf(context);
    context->battle->PublishActionHistory(actor.Player(), ticket, std::move(value), *context->transformator);
}
