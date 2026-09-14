#include <ttrpg/triggers.h>

#include <ttrpg/abilities.h>
#include <ttrpg/activities.h>
#include <ttrpg/conventions.h>
#include <ttrpg/events.h>

#include <functional>
#include <map>
#include <set>
#include <stdexcept>

namespace {

std::map<std::string, const TSchemaModule*> Modules(const TLoadedSchemas& loaded) {
    std::map<std::string, const TSchemaModule*> result;
    for (const auto& [stem, module] : loaded.ByStem) result.emplace(stem, &module);
    result[loaded.PrimaryStem] = &loaded.Primary;
    return result;
}

}

std::unordered_map<std::string, TTriggerInfo> TriggerSymbols(const TLoadedSchemas& loaded) {
    std::unordered_map<std::string, TTriggerInfo> result;
    for (const auto& [stem, module] : Modules(loaded)) {
        for (const auto& trigger : module->Triggers) {
            if (!result.emplace(trigger.Name, TTriggerInfo{stem, &trigger}).second) {
                throw std::runtime_error("duplicate trigger definition '" + trigger.Name + "'");
            }
        }
    }
    return result;
}

TTriggerInfo ResolveTrigger(const std::string& name, const std::string& callerStem, const TLoadedSchemas& loaded) {
    const auto symbols = TriggerSymbols(loaded);
    const auto found = symbols.find(name);
    if (found == symbols.end()) throw std::runtime_error("unknown trigger '" + name + "'");
    std::set<std::string> visible;
    std::function<void(const std::string&)> visit = [&](const std::string& stem) {
        if (!visible.insert(stem).second) return;
        for (const auto& imported : ImportedModuleStems(loaded, stem)) visit(imported);
    };
    visit(callerStem);
    if (!visible.contains(found->second.OwnerStem)) throw std::runtime_error("trigger '" + name + "' is not imported by '" + callerStem + "'");
    return found->second;
}

std::string TriggerTagName(const TTriggerDecl& declaration) {
    return "TTrigger" + declaration.Name;
}

std::string ReactionFunctionName(const std::string& stem, const TAbilityDecl& declaration) {
    TActivityDecl activity;
    activity.Name = declaration.Name;
    return ActivityFunctionName(stem, activity);
}

std::string TriggerArgumentsCppType(const TTriggerDecl& declaration, const TLoadedSchemas& loaded) {
    std::string result = "std::tuple<";
    for (std::size_t i = 1; i < declaration.Parameters.size(); ++i) {
        if (i != 1) result += ", ";
        result += QualifiedCppType(EventArgumentCppType(declaration.Parameters[i].TypeName, loaded), loaded.SymbolTable);
    }
    return result + ">";
}

void ValidateTriggerDefinitions(const TLoadedSchemas& loaded) {
    const auto triggers = TriggerSymbols(loaded);
    const auto abilities = AbilitySymbols(loaded);
    const auto activities = ActivitySymbols(loaded);
    const auto events = EventSymbols(loaded);
    if (!triggers.empty()) {
        for (const auto& name : {"TReactionScope", "TReactionWindowResult", "TReactionExecution"}) {
            if (loaded.SymbolTable.contains(name)) throw std::runtime_error("reserved reaction runtime type '" + std::string(name) + "'");
        }
    }
    std::map<std::string, const TClassDecl*> classes;
    std::set<std::string> enums;
    std::set<std::string> names{
        "void", "int", "bool", "string", "Resource", "BoundedQuantity", "Feature", "Position",
        "roll", "min", "max", "entities", "grid_distance", "same_team", "has_line_of_effect", "sources",
        "position_x", "position_y", "position", "adjacent_positions", "contains_position", "can_occupy", "move", "move_before_current_turn",
        "TReactionScope", "TReactionWindowResult", "TReactionExecution"
    };
    const auto modules = Modules(loaded);
    for (const auto& [stem, module] : modules) {
        names.insert(AbilityRegistrationFunction(stem));
        names.insert(RulesRegistrationFunction(stem));
        for (const auto& value : module->Classes) { names.insert(value.Name); classes[value.Name] = &value; }
        for (const auto& value : module->Enums) { enums.insert(value.Name); names.insert(value.Name); names.insert(value.Name + "FromString"); }
        for (const auto& value : module->Variants) {
            names.insert(value.Name); names.insert(VariantKindEnum(value.Name));
            for (const auto& alternative : value.Alternatives) names.insert(PayloadStructName(value.Name, alternative.Name));
        }
        for (const auto& value : module->Events) names.insert(EventTagName(value));
        for (const auto& value : module->Activities) names.insert(ActivityFunctionName(stem, value));
        for (const auto& value : module->Instances) {
            names.insert(value.Name); names.insert("Create" + value.Name);
            if (value.Parameterized) {
                names.insert("T" + value.Name + "Options"); names.insert("TResolved" + value.Name);
                names.insert("Describe" + value.Name); names.insert("Validate" + value.Name); names.insert("Resolve" + value.Name);
            }
        }
    }
    const auto valueType = [&](const std::string& type, bool parameter) {
        if (IsBuiltinPrimitive(type) || IsBuiltinPosition(type) || enums.contains(type)) return true;
        const auto found = classes.find(type);
        return found != classes.end() && (parameter || !found->second->GameEntity);
    };
    for (const auto& [name, info] : triggers) {
        const auto& declaration = *info.Declaration;
        if (names.contains(name) || activities.contains(name) || abilities.contains(name) || events.contains(name)) {
            throw std::runtime_error("trigger name conflicts with existing symbol '" + name + "'");
        }
        if (!names.insert(TriggerTagName(declaration)).second || triggers.contains(TriggerTagName(declaration))) {
            throw std::runtime_error("generated trigger tag name collision '" + TriggerTagName(declaration) + "'");
        }
        if (!valueType(declaration.ReturnType, false) || declaration.Parameters.empty() ||
            declaration.Parameters.front().TypeName != declaration.ReturnType) {
            throw std::runtime_error("trigger requires a value state type and a matching first parameter");
        }
        std::set<std::string> parameters;
        for (const auto& parameter : declaration.Parameters) {
            if (!valueType(parameter.TypeName, true)) throw std::runtime_error("unsupported trigger parameter type '" + parameter.TypeName + "'");
            if (!parameters.insert(parameter.Name).second) throw std::runtime_error("duplicate trigger parameter '" + parameter.Name + "'");
        }
    }
    for (const auto& [stem, module] : modules) {
        for (const auto& reaction : module->Abilities) {
            if (reaction.Kind != EAbilityDeclarationKind::Reaction || reaction.OwnerType.empty()) continue;
            if (!classes.contains(reaction.OwnerType) || !classes.at(reaction.OwnerType)->GameEntity || reaction.Triggers.empty()) {
                throw std::runtime_error("compiled reaction requires a GameEntity owner and a trigger");
            }
            const auto function = ReactionFunctionName(stem, reaction);
            if (!names.insert(function).second || abilities.contains(function) || activities.contains(function) || triggers.contains(function) || events.contains(function)) {
                throw std::runtime_error("generated reaction function name collision '" + function + "'");
            }
            const TTriggerDecl* signature = nullptr;
            std::set<std::string> subscriptions;
            for (const auto& name : reaction.Triggers) {
                if (!subscriptions.insert(name).second) throw std::runtime_error("duplicate reaction trigger '" + name + "'");
                const auto& trigger = *ResolveTrigger(name, stem, loaded).Declaration;
                if (signature) {
                    bool matches = signature->ReturnType == trigger.ReturnType && signature->Parameters.size() == trigger.Parameters.size();
                    for (std::size_t i = 0; matches && i < trigger.Parameters.size(); ++i) {
                        matches = signature->Parameters[i].TypeName == trigger.Parameters[i].TypeName &&
                            signature->Parameters[i].Name == trigger.Parameters[i].Name;
                    }
                    if (!matches) throw std::runtime_error("reaction trigger signatures must have identical types and parameter names");
                } else {
                    signature = &trigger;
                }
            }
            if (!reaction.SourceName.empty()) {
                for (const auto& parameter : signature->Parameters) if (parameter.Name == reaction.SourceName) {
                    throw std::runtime_error("reaction source conflicts with trigger parameter '" + parameter.Name + "'");
                }
            }
        }
    }
}
