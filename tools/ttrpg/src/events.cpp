#include <ttrpg/events.h>

#include <ttrpg/conventions.h>

#include <stdexcept>
#include <unordered_set>

namespace {

bool EventValueType(const std::string& type, bool allowEntity, const TLoadedSchemas& loaded) {
    return !IsBuiltinPosition(type) && IsValueType(loaded, type, allowEntity);
}

}

TDeclarations<TEventDecl> EventSymbols(const TLoadedSchemas& loaded) {
    return CollectDeclarations(loaded, &TSchemaModule::Events, "event");
}

std::string EventTagName(const TEventDecl& declaration) {
    return "TEvent" + declaration.Name;
}

std::string EventArgumentCppType(const std::string& type, const TLoadedSchemas& loaded) {
    if (IsGameEntity(loaded, type)) return "TGeneratedEntityBinding<" + type + ">";
    return CppTypeFor(type);
}

void ValidateEventDefinitions(const TLoadedSchemas& loaded) {
    const auto events = EventSymbols(loaded);
    for (const auto& [name, info] : events) {
        const auto& event = *info.Declaration;
        if (!IsGameEntity(loaded, event.OwnerType)) throw std::runtime_error("event owner must be a GameEntity: '" + event.OwnerType + "'");
        if (!EventValueType(event.ReturnType, false, loaded)) {
            throw std::runtime_error("event result must be a primitive, enum, or ordinary class: '" + event.ReturnType + "'");
        }
        std::unordered_set<std::string> parameters;
        for (const auto& parameter : event.Parameters) {
            if (!EventValueType(parameter.TypeName, true, loaded)) {
                throw std::runtime_error("unsupported event parameter type '" + parameter.TypeName + "'");
            }
            if (!parameters.insert(parameter.Name).second) throw std::runtime_error("duplicate event parameter '" + parameter.Name + "'");
        }
    }
    for (const auto& [stem, module] : AllModules(loaded)) {
        for (const auto& ability : module->Abilities) {
            std::unordered_set<std::string> handlers;
            for (const auto& hook : ability.Hooks) {
                if (ability.Kind != EAbilityDeclarationKind::Feature) throw std::runtime_error("event hooks require a feature");
                if (!events.contains(hook.EventName)) throw std::runtime_error("unknown hook event '" + hook.EventName + "'");
                if (!handlers.insert(hook.EventName).second) throw std::runtime_error("duplicate feature hook for '" + hook.EventName + "'");
            }
        }
    }
}
