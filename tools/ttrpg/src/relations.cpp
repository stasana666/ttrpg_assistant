#include <ttrpg/relations.h>

#include <ttrpg/abilities.h>
#include <ttrpg/activities.h>
#include <ttrpg/conventions.h>
#include <ttrpg/events.h>
#include <ttrpg/records.h>
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

bool DurationExpression(const expr::TExprNode& expression) {
    if (expression.Kind == expr::ENodeKind::Call && !expression.Lhs &&
        (expression.Text == "seconds" || expression.Text == "minutes")) return true;
    if (expression.Lhs && DurationExpression(*expression.Lhs)) return true;
    if (expression.Rhs && DurationExpression(*expression.Rhs)) return true;
    for (const auto& argument : expression.Args) if (DurationExpression(argument)) return true;
    for (const auto& field : expression.Fields) if (DurationExpression(field.Value)) return true;
    return false;
}

bool DurationStatements(const std::vector<TClassDecl::TStatement>& body) {
    for (const auto& statement : body) {
        if ((statement.Value && DurationExpression(*statement.Value)) ||
            (statement.Target && DurationExpression(*statement.Target)) ||
            (statement.Candidates && DurationExpression(*statement.Candidates)) ||
            DurationStatements(statement.Body) || DurationStatements(statement.ElseBody)) return true;
    }
    return false;
}

}

std::unordered_map<std::string, TRelationInfo> RelationSymbols(const TLoadedSchemas& loaded) {
    std::unordered_map<std::string, TRelationInfo> result;
    for (const auto& [stem, module] : Modules(loaded)) {
        for (const auto& relation : module->Relations) {
            if (!result.emplace(relation.Name, TRelationInfo{stem, &relation}).second) {
                throw std::runtime_error("duplicate relation '" + relation.Name + "'");
            }
        }
    }
    return result;
}

TRelationInfo ResolveRelation(const std::string& name, const std::string& callerStem, const TLoadedSchemas& loaded) {
    const auto symbols = RelationSymbols(loaded);
    const auto found = symbols.find(name);
    if (found == symbols.end()) throw std::runtime_error("unknown relation '" + name + "'");
    std::set<std::string> visible;
    std::function<void(const std::string&)> visit = [&](const std::string& stem) {
        if (!visible.insert(stem).second) return;
        for (const auto& imported : ImportedModuleStems(loaded, stem)) visit(imported);
    };
    visit(callerStem);
    if (!visible.contains(found->second.OwnerStem)) throw std::runtime_error("relation '" + name + "' is not imported by '" + callerStem + "'");
    return found->second;
}

std::string RelationTagName(const TRelationDecl& declaration) {
    return "TRelation" + declaration.Name;
}

void ValidateRelationDefinitions(const TLoadedSchemas& loaded) {
    const auto relations = RelationSymbols(loaded);
    std::map<std::string, const TClassDecl*> classes;
    std::set<std::string> enums;
    const std::set<std::string> runtimeNames{
        "Duration", "TGameDuration", "TGameClock", "TGameTimeRules", "TRelationRegistry", "TTimedRelationStore",
        "seconds", "minutes", "GameSeconds", "GameMinutes", "HasRelation", "GetRelation", "SetRelation", "EraseRelation"
    };
    std::set<std::string> names{
        "void", "int", "bool", "string", "Resource", "BoundedQuantity", "Feature", "Position",
        "roll", "min", "max", "entities", "grid_distance", "same_team", "has_line_of_effect", "sources",
        "position_x", "position_y", "position", "adjacent_positions", "contains_position", "can_occupy", "move",
        "move_before_current_turn", "same_source"
    };
    const auto add = [&](const std::string& name) {
        if (runtimeNames.contains(name)) throw std::runtime_error("reserved relation or duration symbol '" + name + "'");
        names.insert(name);
    };
    for (const auto& [stem, module] : Modules(loaded)) {
        add(AbilityRegistrationFunction(stem));
        add(RulesRegistrationFunction(stem));
        for (const auto& value : module->Classes) { classes[value.Name] = &value; add(value.Name); }
        for (const auto& value : module->Enums) { enums.insert(value.Name); add(value.Name); add(value.Name + "FromString"); }
        for (const auto& value : module->Variants) {
            add(value.Name); add(VariantKindEnum(value.Name));
            for (const auto& alternative : value.Alternatives) add(PayloadStructName(value.Name, alternative.Name));
        }
        for (const auto& value : module->Events) { add(value.Name); add(EventTagName(value)); }
        for (const auto& value : module->Triggers) { add(value.Name); add(TriggerTagName(value)); }
        for (const auto& value : module->Records) { add(value.Name); add(RecordTagName(value)); }
        for (const auto& value : module->Activities) { add(value.Name); add(ActivityFunctionName(stem, value)); }
        for (const auto& value : module->Abilities) {
            add(value.Name);
            if (!value.Triggers.empty()) add(ReactionFunctionName(stem, value));
        }
        for (const auto& value : module->Instances) {
            add(value.Name); add("Create" + value.Name);
            if (value.Parameterized) {
                add("T" + value.Name + "Options"); add("TResolved" + value.Name);
                add("Describe" + value.Name); add("Validate" + value.Name); add("Resolve" + value.Name);
            }
        }
    }
    names.insert(runtimeNames.begin(), runtimeNames.end());
    for (const auto& [name, info] : relations) {
        const auto& declaration = *info.Declaration;
        if (names.contains(name)) throw std::runtime_error("relation name conflicts with existing symbol '" + name + "'");
        const auto tag = RelationTagName(declaration);
        if (!names.insert(tag).second || relations.contains(tag)) throw std::runtime_error("generated relation tag name collision '" + tag + "'");
        if (!classes.contains(declaration.OwnerType) || !classes.at(declaration.OwnerType)->GameEntity) {
            throw std::runtime_error("relation owner must be a GameEntity");
        }
        if (declaration.Parameters.size() != 1) throw std::runtime_error("relation requires exactly one GameEntity key");
        const auto& key = declaration.Parameters.front();
        if (!classes.contains(key.TypeName) || !classes.at(key.TypeName)->GameEntity) {
            throw std::runtime_error("relation key must be a GameEntity");
        }
        const auto cls = classes.find(declaration.ValueType);
        const bool ordinary = cls != classes.end() && !cls->second->GameEntity;
        if (!ordinary && !IsBuiltinPrimitive(declaration.ValueType) && !IsBuiltinPosition(declaration.ValueType) &&
            !enums.contains(declaration.ValueType)) throw std::runtime_error("relation value requires a primitive, enum, Position, or ordinary class");
    }
}

bool UsesDurationSyntax(const TSchemaModule& module) {
    for (const auto& value : module.Classes) {
        for (const auto& field : value.Fields) if (field.Init && DurationExpression(*field.Init)) return true;
        for (const auto& method : value.Methods) if (DurationStatements(method.Body)) return true;
    }
    for (const auto& value : module.Variants) for (const auto& alternative : value.Alternatives) {
        for (const auto& field : alternative.Fields) if (field.Init && DurationExpression(*field.Init)) return true;
    }
    for (const auto& value : module.Abilities) {
        if (DurationStatements(value.Body)) return true;
        for (const auto& hook : value.Hooks) if (DurationStatements(hook.Body)) return true;
    }
    for (const auto& value : module.Activities) if (DurationStatements(value.Body)) return true;
    for (const auto& value : module.Instances) {
        if (DurationExpression(value.Value) || DurationStatements(value.Requirements)) return true;
        for (const auto& parameter : value.Parameters) {
            if ((parameter.Field.Init && DurationExpression(*parameter.Field.Init)) ||
                (parameter.Pool && DurationExpression(*parameter.Pool)) ||
                (parameter.When && DurationExpression(*parameter.When))) return true;
        }
    }
    return false;
}
