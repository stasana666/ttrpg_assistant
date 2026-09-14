#include <pf2e_engine/actions/reaction_window.h>

#include <pf2e_engine/battle.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/player.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

bool TReactionScope::Used(const TGeneratedEntityView& actor, std::string_view key) const
{
    if (key.empty()) {
        return false;
    }
    const auto* identity = &actor.Player();
    return std::ranges::any_of(uses_, [&](const TUse& use) {
        return use.Identity == identity && use.Actor == actor && use.Key == key;
    });
}

TReactionScope TReactionScope::WithUse(const TGeneratedEntityView& actor, std::string key) const
{
    auto result = *this;
    if (!key.empty() && !Used(actor, key)) {
        result.uses_.push_back({actor.ReadOnly(), &actor.Player(), std::move(key)});
    }
    return result;
}

namespace reaction_window_detail {

std::shared_ptr<const TEnvironment> CreateEnvironment(
    const std::shared_ptr<TActionContext>& parent, std::string_view trigger_id)
{
    ValidateActionActor(parent);
    if (parent->battle == nullptr || parent->io_system == nullptr || trigger_id.empty()) {
        throw std::invalid_argument("reaction window requires a battle, interaction and a trigger");
    }
    auto trace = parent->reaction_trace ? *parent->reaction_trace : std::vector<std::string>{};
    if (std::ranges::find(trace, trigger_id) != trace.end()) {
        throw std::logic_error("cyclic reaction window");
    }
    trace.emplace_back(trigger_id);
    auto services = std::make_shared<TActionContext>(TActionContext{
        .battle = parent->battle,
        .dice_roller = parent->dice_roller,
        .transformator = parent->transformator,
        .io_system = parent->io_system,
        .scheduler = parent->scheduler,
        .effect_manager = parent->effect_manager,
    });
    services->reaction_trace = std::make_shared<const std::vector<std::string>>(std::move(trace));
    std::vector<TGeneratedEntityView> participants;
    for (auto* player : parent->battle->GetIfPlayers([](const TPlayer*) { return true; })) {
        participants.push_back(BindGeneratedActionEntity(*player, parent->battle));
    }
    return std::make_shared<const TEnvironment>(TEnvironment{
        std::move(services), &parent->battle->AbilityRules(), std::move(participants)});
}

std::shared_ptr<TActionContext> ActorContext(const TEnvironment& environment,
                                           const TGeneratedEntityView& actor)
{
    auto context = std::make_shared<TActionContext>(*environment.Services);
    ValidateActionEntityMembership(context, actor);
    context->game_object_registry = std::make_shared<TGameObjectRegistry>();
    const auto self = TGameObjectIdManager::Instance().Register("self");
    context->game_object_registry->Add(self, &actor.MutablePlayer());
    ValidateActionActorBinding(context, actor);
    return context;
}

bool ActorAvailable(const TEnvironment& environment, const TGeneratedEntityView& actor)
{
    TGeneratedWorldView(environment.Services->battle).Validate(actor);
    return actor.Player().GetCreature()->CanAct();
}

TAbilityCollector Collect(const TGeneratedEntityView& actor, const TAbilityRules& rules)
{
    return CollectAbilities(*actor.Player().GetCreature(), rules.Catalog);
}

void BindExecution(const std::shared_ptr<TActionContext>& context,
                   const TGeneratedEntityView& actor, const TAbilityRules& rules,
                   const TGrantedAbility& grant, std::shared_ptr<const void> execution)
{
    context->owned_reaction = std::move(execution);
    context->ability_source = grant.Source;
    context->validate_ability = [actor, catalog = &rules.Catalog,
                                definition = grant.DefinitionId, source = grant.Source]() {
        const auto abilities = CollectAbilities(*actor.Player().GetCreature(), *catalog);
        return std::ranges::any_of(abilities.Grants(), [&](const TGrantedAbility& current) {
            return current.DefinitionId == definition && current.Source == source;
        });
    };
}

}
