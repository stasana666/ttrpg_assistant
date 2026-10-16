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
        AnyNode(statements, [&](const expr::TExprNode& node) {
            if (node.Kind != expr::ENodeKind::Call || node.Lhs) return false;
            if (activities.contains(node.Text)) {
                ResolveActivity(node.Text, stem, loaded);
                edges[source].insert("activity " + node.Text);
            }
            if (triggers.contains(node.Text)) {
                ResolveTrigger(node.Text, stem, loaded);
                edges[source].insert("trigger " + node.Text);
            }
            return false;
        });
    };
    for (const auto& [name, info] : activities) body("activity " + name, info.OwnerStem, info.Declaration->Body);
    for (const auto& [stem, module] : AllModules(loaded)) {
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
