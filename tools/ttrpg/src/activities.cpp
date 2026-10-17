#include <ttrpg/activities.h>

#include <ttrpg/abilities.h>
#include <ttrpg/conventions.h>
#include <ttrpg/callable_graph.h>

#include <functional>
#include <stdexcept>
#include <unordered_set>

TDeclarations<TActivityDecl> ActivitySymbols(const TLoadedSchemas& loaded) {
    return CollectDeclarations(loaded, &TSchemaModule::Activities, "activity");
}

TActivityInfo ResolveActivity(const std::string& name, const std::string& callerStem, const TLoadedSchemas& loaded) {
    return ResolveDeclaration(loaded, &TSchemaModule::Activities, "activity", name, callerStem);
}

std::string ActivityFunctionName(const std::string& ownerStem, const TActivityDecl& declaration) {
    auto name = RulesRegistrationFunction(ownerStem);
    name.erase(0, std::string("Register").size());
    name.resize(name.size() - std::string("Rules").size());
    return "Run" + name + declaration.Name;
}

void ValidateActivityDefinitions(const TLoadedSchemas& loaded) {
    for (const auto& [name, info] : ActivitySymbols(loaded)) {
        const auto& activity = *info.Declaration;
        if (!IsGameEntity(loaded, activity.OwnerType)) throw std::runtime_error("activity owner must be a GameEntity");
        if (activity.ReturnType != "void" && !IsValueType(loaded, activity.ReturnType, false)) {
            throw std::runtime_error("activity result requires void, primitive, enum, Position, or ordinary class type");
        }
        std::unordered_set<std::string> parameters;
        for (const auto& parameter : activity.Parameters) {
            if (!IsValueType(loaded, parameter.TypeName, true)) throw std::runtime_error("unsupported activity parameter type '" + parameter.TypeName + "'");
            if (!parameters.insert(parameter.Name).second) throw std::runtime_error("duplicate activity parameter '" + parameter.Name + "'");
        }
    }
    ValidateCallableGraph(loaded);
}

std::string ActivityClassResultDependency(const std::string& name, const TLoadedSchemas& loaded) {
    const auto activities = ActivitySymbols(loaded);
    std::unordered_set<std::string> visited;
    std::function<std::string(const std::string&)> visit = [&](const std::string& current) -> std::string {
        if (!visited.insert(current).second) return {};
        const auto& activity = *activities.at(current).Declaration;
        if (const auto* result = FindClass(loaded, activity.ReturnType); result && !result->GameEntity) return current;
        std::string result;
        AnyNode(activity.Body, [&](const expr::TExprNode& node) {
            if (node.Kind == expr::ENodeKind::Call && !node.Lhs && activities.contains(node.Text)) result = visit(node.Text);
            return !result.empty();
        });
        return result;
    };
    return visit(name);
}
