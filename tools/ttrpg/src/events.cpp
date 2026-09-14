#include <ttrpg/events.h>

#include <ttrpg/abilities.h>
#include <ttrpg/conventions.h>

#include <map>
#include <stdexcept>
#include <unordered_set>

namespace {

std::map<std::string, const TSchemaModule*> Modules(const TLoadedSchemas& loaded) {
    std::map<std::string, const TSchemaModule*> result;
    for (const auto& [stem, module] : loaded.ByStem) result.emplace(stem, &module);
    result[loaded.PrimaryStem] = &loaded.Primary;
    return result;
}

const TClassDecl* FindClass(const std::string& name, const TLoadedSchemas& loaded) {
    for (const auto& [stem, module] : Modules(loaded)) {
        for (const auto& declaration : module->Classes) if (declaration.Name == name) return &declaration;
    }
    return nullptr;
}

bool ValueType(const std::string& name, bool allowEntity, const TLoadedSchemas& loaded) {
    if (IsBuiltinPrimitive(name)) return true;
    if (const auto type = loaded.SymbolTable.find(name); type != loaded.SymbolTable.end() && type->second.Kind == ETypeKind::Enum) return true;
    const auto* declaration = FindClass(name, loaded);
    return declaration && (allowEntity || !declaration->GameEntity);
}

}

std::unordered_map<std::string, TEventInfo> EventSymbols(const TLoadedSchemas& loaded) {
    std::unordered_map<std::string, TEventInfo> result;
    for (const auto& [stem, module] : Modules(loaded)) {
        for (const auto& event : module->Events) {
            if (!result.emplace(event.Name, TEventInfo{stem, &event}).second) {
                throw std::runtime_error("duplicate event definition '" + event.Name + "'");
            }
        }
    }
    return result;
}

std::string EventTagName(const TEventDecl& declaration) {
    return "TEvent" + declaration.Name;
}

std::string EventArgumentCppType(const std::string& type, const TLoadedSchemas& loaded) {
    const auto* declaration = FindClass(type, loaded);
    if (declaration && declaration->GameEntity) return "TGeneratedEntityBinding<" + type + ">";
    return CppTypeFor(type);
}

void ValidateEventDefinitions(const TLoadedSchemas& loaded) {
    const auto events = EventSymbols(loaded);
    const auto abilities = AbilitySymbols(loaded);
    for (const auto& [name, info] : events) {
        const auto& event = *info.Declaration;
        if (loaded.SymbolTable.contains(name) || abilities.contains(name) || IsBuiltinPrimitive(name) ||
            name == "Feature" || name == "Resource" || name == "BoundedQuantity" ||
            name == "roll" || name == "min" || name == "max" || name == "entities" ||
            name == "grid_distance" || name == "same_team" || name == "has_line_of_effect" ||
            name == "position_x" || name == "position_y") {
            throw std::runtime_error("event name conflicts with existing symbol '" + name + "'");
        }
        if (loaded.SymbolTable.contains(EventTagName(event))) {
            throw std::runtime_error("generated event tag conflicts with type '" + EventTagName(event) + "'");
        }
        const auto* owner = FindClass(event.OwnerType, loaded);
        if (!owner || !owner->GameEntity) throw std::runtime_error("event owner must be a GameEntity: '" + event.OwnerType + "'");
        if (!ValueType(event.ReturnType, false, loaded)) {
            throw std::runtime_error("event result must be a primitive, enum, or ordinary class: '" + event.ReturnType + "'");
        }
        std::unordered_set<std::string> parameters;
        for (const auto& parameter : event.Parameters) {
            if (!ValueType(parameter.TypeName, true, loaded)) {
                throw std::runtime_error("unsupported event parameter type '" + parameter.TypeName + "'");
            }
            if (!parameters.insert(parameter.Name).second) throw std::runtime_error("duplicate event parameter '" + parameter.Name + "'");
        }
    }
    for (const auto& [stem, module] : Modules(loaded)) {
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
