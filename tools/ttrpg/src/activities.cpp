#include <ttrpg/activities.h>

#include <ttrpg/abilities.h>
#include <ttrpg/conventions.h>
#include <ttrpg/events.h>
#include <ttrpg/callable_graph.h>

#include <functional>
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

}

std::unordered_map<std::string, TActivityInfo> ActivitySymbols(const TLoadedSchemas& loaded) {
    std::unordered_map<std::string, TActivityInfo> result;
    for (const auto& [stem, module] : Modules(loaded)) {
        for (const auto& activity : module->Activities) {
            if (!result.emplace(activity.Name, TActivityInfo{stem, &activity}).second) {
                throw std::runtime_error("duplicate activity definition '" + activity.Name + "'");
            }
        }
    }
    return result;
}

TActivityInfo ResolveActivity(const std::string& name, const std::string& callerStem, const TLoadedSchemas& loaded) {
    const auto symbols = ActivitySymbols(loaded);
    const auto found = symbols.find(name);
    if (found == symbols.end()) throw std::runtime_error("unknown activity '" + name + "'");
    std::unordered_set<std::string> visible;
    std::function<void(const std::string&)> visit = [&](const std::string& stem) {
        if (!visible.insert(stem).second) return;
        for (const auto& imported : ImportedModuleStems(loaded, stem)) visit(imported);
    };
    visit(callerStem);
    if (!visible.contains(found->second.OwnerStem)) {
        throw std::runtime_error("activity '" + name + "' is not imported by module '" + callerStem + "'");
    }
    return found->second;
}

std::string ActivityFunctionName(const std::string& ownerStem, const TActivityDecl& declaration) {
    auto name = RulesRegistrationFunction(ownerStem);
    name.erase(0, std::string("Register").size());
    name.resize(name.size() - std::string("Rules").size());
    return "Run" + name + declaration.Name;
}

void ValidateActivityDefinitions(const TLoadedSchemas& loaded) {
    const auto activities = ActivitySymbols(loaded);
    const auto abilities = AbilitySymbols(loaded);
    const auto events = EventSymbols(loaded);
    std::unordered_map<std::string, const TClassDecl*> classes;
    std::unordered_set<std::string> enums;
    std::unordered_set<std::string> names;
    const std::unordered_set<std::string> builtins{
        "void", "int", "bool", "string", "Resource", "BoundedQuantity", "Feature", "Position",
        "roll", "min", "max", "entities", "grid_distance", "same_team", "has_line_of_effect",
        "sources", "position_x", "position_y", "position", "adjacent_positions", "contains_position", "can_occupy", "move", "move_before_current_turn"
    };
    for (const auto& [stem, module] : Modules(loaded)) {
        names.insert(AbilityRegistrationFunction(stem));
        names.insert(RulesRegistrationFunction(stem));
        for (const auto& type : module->Classes) { classes[type.Name] = &type; names.insert(type.Name); }
        for (const auto& type : module->Enums) { enums.insert(type.Name); names.insert(type.Name); names.insert(type.Name + "FromString"); }
        for (const auto& type : module->Variants) {
            names.insert(type.Name);
            names.insert(VariantKindEnum(type.Name));
            for (const auto& alternative : type.Alternatives) names.insert(PayloadStructName(type.Name, alternative.Name));
        }
        for (const auto& event : module->Events) names.insert(EventTagName(event));
        for (const auto& instance : module->Instances) { names.insert(instance.Name); names.insert("Create" + instance.Name); }
    }
    const auto valueType = [&](const std::string& type, bool parameter) {
        if (IsBuiltinPrimitive(type) || enums.contains(type) || IsBuiltinPosition(type)) return true;
        const auto found = classes.find(type);
        return found != classes.end() && (parameter || !found->second->GameEntity);
    };
    for (const auto& [name, info] : activities) {
        const auto& activity = *info.Declaration;
        if (names.contains(name) || builtins.contains(name) || abilities.contains(name) || events.contains(name)) {
            throw std::runtime_error("activity name conflicts with existing symbol '" + name + "'");
        }
        const auto owner = classes.find(activity.OwnerType);
        if (owner == classes.end() || !owner->second->GameEntity) throw std::runtime_error("activity owner must be a GameEntity");
        if (activity.ReturnType != "void" && !valueType(activity.ReturnType, false)) {
            throw std::runtime_error("activity result requires void, primitive, enum, Position, or ordinary class type");
        }
        std::unordered_set<std::string> parameters;
        for (const auto& parameter : activity.Parameters) {
            if (!valueType(parameter.TypeName, true)) throw std::runtime_error("unsupported activity parameter type '" + parameter.TypeName + "'");
            if (!parameters.insert(parameter.Name).second) throw std::runtime_error("duplicate activity parameter '" + parameter.Name + "'");
        }
    }
    for (const auto& [name, info] : activities) {
        const auto function = ActivityFunctionName(info.OwnerStem, *info.Declaration);
        if (!names.insert(function).second || activities.contains(function) || builtins.contains(function) ||
            abilities.contains(function) || events.contains(function)) {
            throw std::runtime_error("generated activity function name collision '" + function + "'");
        }
    }
    ValidateCallableGraph(loaded);
}

std::string ActivityClassResultDependency(const std::string& name, const TLoadedSchemas& loaded) {
    const auto activities = ActivitySymbols(loaded);
    std::unordered_set<std::string> classes;
    for (const auto& [stem, module] : Modules(loaded)) {
        for (const auto& type : module->Classes) if (!type.GameEntity) classes.insert(type.Name);
    }
    std::unordered_set<std::string> visited;
    std::function<std::string(const std::string&)> visit;
    visit = [&](const std::string& current) -> std::string {
        if (!visited.insert(current).second) return {};
        const auto& activity = *activities.at(current).Declaration;
        if (classes.contains(activity.ReturnType)) return current;
        std::string result;
        std::function<void(const expr::TExprNode&)> expression;
        expression = [&](const expr::TExprNode& node) {
            if (!result.empty()) return;
            if (node.Kind == expr::ENodeKind::Call && !node.Lhs && activities.contains(node.Text)) result = visit(node.Text);
            if (node.Lhs) expression(*node.Lhs);
            if (node.Rhs) expression(*node.Rhs);
            for (const auto& argument : node.Args) expression(argument);
            for (const auto& field : node.Fields) expression(field.Value);
        };
        std::function<void(const std::vector<TClassDecl::TStatement>&)> statements;
        statements = [&](const auto& body) {
            for (const auto& statement : body) {
                if (statement.Value) expression(*statement.Value);
                if (statement.Target) expression(*statement.Target);
                if (statement.Candidates) expression(*statement.Candidates);
                statements(statement.Body);
                statements(statement.ElseBody);
            }
        };
        statements(activity.Body);
        return result;
    };
    return visit(name);
}
