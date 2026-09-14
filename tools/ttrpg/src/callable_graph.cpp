#include <ttrpg/callable_graph.h>

#include <ttrpg/activities.h>
#include <ttrpg/triggers.h>

#include <functional>
#include <map>
#include <set>
#include <stdexcept>

void ValidateCallableGraph(const TLoadedSchemas& loaded) {
    const auto activities = ActivitySymbols(loaded);
    const auto triggers = TriggerSymbols(loaded);
    std::map<std::string, std::set<std::string>> edges;
    const auto body = [&](const std::string& source, const std::string& stem, const auto& statements) {
        std::function<void(const expr::TExprNode&)> expression;
        expression = [&](const expr::TExprNode& node) {
            if (node.Kind == expr::ENodeKind::Call && !node.Lhs) {
                if (activities.contains(node.Text)) {
                    ResolveActivity(node.Text, stem, loaded);
                    edges[source].insert("activity " + node.Text);
                }
                if (triggers.contains(node.Text)) {
                    ResolveTrigger(node.Text, stem, loaded);
                    edges[source].insert("trigger " + node.Text);
                }
            }
            if (node.Lhs) expression(*node.Lhs);
            if (node.Rhs) expression(*node.Rhs);
            for (const auto& value : node.Args) expression(value);
            for (const auto& value : node.Fields) expression(value.Value);
        };
        std::function<void(const std::vector<TClassDecl::TStatement>&)> inspect;
        inspect = [&](const auto& values) {
            for (const auto& statement : values) {
                if (statement.Value) expression(*statement.Value);
                if (statement.Target) expression(*statement.Target);
                if (statement.Candidates) expression(*statement.Candidates);
                inspect(statement.Body); inspect(statement.ElseBody);
            }
        };
        inspect(statements);
    };
    for (const auto& [name, info] : activities) body("activity " + name, info.OwnerStem, info.Declaration->Body);
    std::map<std::string, const TSchemaModule*> modules;
    for (const auto& [stem, module] : loaded.ByStem) modules[stem] = &module;
    modules[loaded.PrimaryStem] = &loaded.Primary;
    for (const auto& [stem, module] : modules) {
        for (const auto& ability : module->Abilities) {
            if (ability.OwnerType.empty()) continue;
            const auto name = std::string(ability.Kind == EAbilityDeclarationKind::Reaction ? "reaction " : "action ") + ability.Name;
            for (const auto& trigger : ability.Triggers) edges["trigger " + trigger].insert(name);
            body(name, stem, ability.Body);
        }
    }
    std::map<std::string, int> marks;
    std::vector<std::string> path;
    std::function<void(const std::string&)> visit = [&](const std::string& name) {
        if (marks[name] == 2) return;
        if (marks[name] == 1) {
            std::string cycle;
            for (const auto& step : path) cycle += step + " -> ";
            throw std::runtime_error("recursive activity calls or reaction windows: " + cycle + name);
        }
        marks[name] = 1;
        path.push_back(name);
        for (const auto& target : edges[name]) visit(target);
        path.pop_back();
        marks[name] = 2;
    };
    for (const auto& [name, targets] : edges) visit(name);
}
