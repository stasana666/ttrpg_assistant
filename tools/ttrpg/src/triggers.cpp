#include <ttrpg/triggers.h>

#include <ttrpg/activities.h>
#include <ttrpg/conventions.h>
#include <ttrpg/events.h>

#include <set>
#include <stdexcept>

TDeclarations<TTriggerDecl> TriggerSymbols(const TLoadedSchemas& loaded) {
    return CollectDeclarations(loaded, &TSchemaModule::Triggers, "trigger");
}

TTriggerInfo ResolveTrigger(const std::string& name, const std::string& callerStem, const TLoadedSchemas& loaded) {
    return ResolveDeclaration(loaded, &TSchemaModule::Triggers, "trigger", name, callerStem);
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
    for (const auto& [name, info] : TriggerSymbols(loaded)) {
        const auto& declaration = *info.Declaration;
        if (!IsValueType(loaded, declaration.ReturnType, false) || declaration.Parameters.empty() ||
            declaration.Parameters.front().TypeName != declaration.ReturnType) {
            throw std::runtime_error("trigger requires a value state type and a matching first parameter");
        }
        std::set<std::string> parameters;
        for (const auto& parameter : declaration.Parameters) {
            if (!IsValueType(loaded, parameter.TypeName, true)) throw std::runtime_error("unsupported trigger parameter type '" + parameter.TypeName + "'");
            if (!parameters.insert(parameter.Name).second) throw std::runtime_error("duplicate trigger parameter '" + parameter.Name + "'");
        }
    }
    for (const auto& [stem, module] : AllModules(loaded)) {
        for (const auto& reaction : module->Abilities) {
            if (reaction.Kind != EAbilityDeclarationKind::Reaction || reaction.OwnerType.empty()) continue;
            if (!IsGameEntity(loaded, reaction.OwnerType) || reaction.Triggers.empty()) {
                throw std::runtime_error("compiled reaction requires a GameEntity owner and a trigger");
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
            for (const auto& parameter : signature->Parameters) {
                if (parameter.Name == reaction.SourceName) {
                    throw std::runtime_error("reaction source conflicts with trigger parameter '" + parameter.Name + "'");
                }
            }
        }
    }
}
