#include <ttrpg/records.h>

#include <ttrpg/abilities.h>
#include <ttrpg/activities.h>
#include <ttrpg/conventions.h>
#include <ttrpg/events.h>
#include <ttrpg/triggers.h>

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

void CheckPublication(const std::vector<TClassDecl::TStatement>& body, bool rootAction) {
    using K = TClassDecl::TStatement::EKind;
    for (std::size_t i = 0; i < body.size(); ++i) {
        const auto& statement = body[i];
        if (statement.Kind == K::Publish && (!rootAction || i + 1 != body.size())) {
            throw std::runtime_error("publish must be the final statement of a root action body");
        }
        CheckPublication(statement.Body, false);
        CheckPublication(statement.ElseBody, false);
    }
}

}

std::unordered_map<std::string, TRecordInfo> RecordSymbols(const TLoadedSchemas& loaded) {
    std::unordered_map<std::string, TRecordInfo> result;
    for (const auto& [stem, module] : Modules(loaded)) {
        for (const auto& record : module->Records) {
            if (!result.emplace(record.Name, TRecordInfo{stem, &record}).second) {
                throw std::runtime_error("duplicate completed action record '" + record.Name + "'");
            }
        }
    }
    return result;
}

TRecordInfo ResolveRecord(const std::string& name, const std::string& callerStem, const TLoadedSchemas& loaded) {
    const auto symbols = RecordSymbols(loaded);
    const auto found = symbols.find(name);
    if (found == symbols.end()) throw std::runtime_error("unknown completed action record '" + name + "'");
    std::set<std::string> visible;
    std::function<void(const std::string&)> visit = [&](const std::string& stem) {
        if (!visible.insert(stem).second) return;
        for (const auto& imported : ImportedModuleStems(loaded, stem)) visit(imported);
    };
    visit(callerStem);
    if (!visible.contains(found->second.OwnerStem)) throw std::runtime_error("record '" + name + "' is not imported by '" + callerStem + "'");
    return found->second;
}

std::string RecordTagName(const TRecordDecl& declaration) {
    return "TRecord" + declaration.Name;
}

void ValidateRecordDefinitions(const TLoadedSchemas& loaded) {
    const auto records = RecordSymbols(loaded);
    const auto modules = Modules(loaded);
    std::map<std::string, const TClassDecl*> classes;
    std::set<std::string> enums;
    std::set<std::string> names{
        "void", "int", "bool", "string", "Resource", "BoundedQuantity", "Feature", "Position",
        "roll", "min", "max", "entities", "grid_distance", "same_team", "has_line_of_effect", "sources",
        "position_x", "position_y", "position", "adjacent_positions", "contains_position", "can_occupy", "move", "move_before_current_turn", "same_source",
        "TActionTicket", "TActionRecordStamp", "TActionRecordRegistry", "TActionHistory", "TRecordedSource", "TRecordedEntity"
    };
    for (const auto& [stem, module] : modules) {
        names.insert(AbilityRegistrationFunction(stem));
        names.insert(RulesRegistrationFunction(stem));
        for (const auto& value : module->Classes) { classes[value.Name] = &value; names.insert(value.Name); }
        for (const auto& value : module->Enums) { enums.insert(value.Name); names.insert(value.Name); names.insert(value.Name + "FromString"); }
        for (const auto& value : module->Variants) {
            names.insert(value.Name); names.insert(VariantKindEnum(value.Name));
            for (const auto& alternative : value.Alternatives) names.insert(PayloadStructName(value.Name, alternative.Name));
        }
        for (const auto& value : module->Events) { names.insert(value.Name); names.insert(EventTagName(value)); }
        for (const auto& value : module->Triggers) { names.insert(value.Name); names.insert(TriggerTagName(value)); }
        for (const auto& value : module->Activities) { names.insert(value.Name); names.insert(ActivityFunctionName(stem, value)); CheckPublication(value.Body, false); }
        for (const auto& value : module->Abilities) {
            names.insert(value.Name);
            if (!value.Triggers.empty()) names.insert(ReactionFunctionName(stem, value));
            CheckPublication(value.Body, value.Kind == EAbilityDeclarationKind::Action && !value.OwnerType.empty());
            for (const auto& hook : value.Hooks) CheckPublication(hook.Body, false);
        }
        for (const auto& value : module->Classes) for (const auto& method : value.Methods) CheckPublication(method.Body, false);
        for (const auto& value : module->Instances) {
            names.insert(value.Name); names.insert("Create" + value.Name);
            if (value.Parameterized) {
                names.insert("T" + value.Name + "Options"); names.insert("TResolved" + value.Name);
                names.insert("Describe" + value.Name); names.insert("Validate" + value.Name); names.insert("Resolve" + value.Name);
            }
        }
    }
    for (const auto* name : {"TActionTicket", "TActionRecordStamp", "TActionRecordRegistry", "TActionHistory", "TRecordedSource", "TRecordedEntity"}) {
        if (loaded.SymbolTable.contains(name)) throw std::runtime_error("reserved history runtime type '" + std::string(name) + "'");
    }
    if (loaded.SymbolTable.contains("same_source") || AbilitySymbols(loaded).contains("same_source") ||
        ActivitySymbols(loaded).contains("same_source") || EventSymbols(loaded).contains("same_source") || TriggerSymbols(loaded).contains("same_source")) {
        throw std::runtime_error("same_source is reserved for source identity comparison");
    }
    for (const auto& [name, info] : records) {
        const auto& declaration = *info.Declaration;
        if (names.contains(name)) throw std::runtime_error("record name conflicts with existing symbol '" + name + "'");
        if (!names.insert(RecordTagName(declaration)).second || records.contains(RecordTagName(declaration))) {
            throw std::runtime_error("generated record tag name collision '" + RecordTagName(declaration) + "'");
        }
        if (!classes.contains(declaration.OwnerType) || !classes.at(declaration.OwnerType)->GameEntity) throw std::runtime_error("record owner must be a GameEntity");
        if (!classes.contains(declaration.SourceType) || classes.at(declaration.SourceType)->GameEntity) throw std::runtime_error("record source must be an ordinary class");
        std::set<std::string> parameters{declaration.SourceName};
        const std::set<std::string> reserved{"TOwner", "TSource", "TArguments", "TStored", "TInputs", "kId"};
        if (reserved.contains(declaration.SourceName)) throw std::runtime_error("reserved record source name '" + declaration.SourceName + "'");
        for (const auto& parameter : declaration.Parameters) {
            if (!IsBuiltinPrimitive(parameter.TypeName) && !IsBuiltinPosition(parameter.TypeName) &&
                !enums.contains(parameter.TypeName) && !classes.contains(parameter.TypeName)) {
                throw std::runtime_error("unsupported record parameter type '" + parameter.TypeName + "'");
            }
            if (reserved.contains(parameter.Name) || !parameters.insert(parameter.Name).second) throw std::runtime_error("duplicate or reserved record parameter '" + parameter.Name + "'");
        }
    }
    for (const auto& [stem, module] : modules) for (const auto& ability : module->Abilities) {
        if (ability.PreviousRecord.empty()) continue;
        if (ability.Kind != EAbilityDeclarationKind::Action || ability.OwnerType.empty()) throw std::runtime_error("after requires a root action body");
        const auto& record = *ResolveRecord(ability.PreviousRecord, stem, loaded).Declaration;
        if (record.OwnerType != ability.OwnerType) throw std::runtime_error("after record owner does not match action owner");
        if (ability.PreviousName.empty() || ability.PreviousName == ability.SourceName) throw std::runtime_error("after binding conflicts with action source");
    }
}
