#include <ttrpg/relations.h>

#include <ttrpg/conventions.h>

#include <stdexcept>

namespace {

bool IsDurationCall(const expr::TExprNode& node) {
    return node.Kind == expr::ENodeKind::Call && !node.Lhs && (node.Text == "seconds" || node.Text == "minutes");
}

bool UsesDuration(const std::optional<expr::TExprNode>& expression) {
    return expression && AnyNode(*expression, IsDurationCall);
}

}

TDeclarations<TRelationDecl> RelationSymbols(const TLoadedSchemas& loaded) {
    return CollectDeclarations(loaded, &TSchemaModule::Relations, "relation");
}

TRelationInfo ResolveRelation(const std::string& name, const std::string& callerStem, const TLoadedSchemas& loaded) {
    return ResolveDeclaration(loaded, &TSchemaModule::Relations, "relation", name, callerStem);
}

std::string RelationTagName(const TRelationDecl& declaration) {
    return "TRelation" + declaration.Name;
}

void ValidateRelationDefinitions(const TLoadedSchemas& loaded) {
    for (const auto& [name, info] : RelationSymbols(loaded)) {
        const auto& declaration = *info.Declaration;
        if (!IsGameEntity(loaded, declaration.OwnerType)) throw std::runtime_error("relation owner must be a GameEntity");
        if (declaration.Parameters.size() != 1) throw std::runtime_error("relation requires exactly one GameEntity key");
        if (!IsGameEntity(loaded, declaration.Parameters.front().TypeName)) throw std::runtime_error("relation key must be a GameEntity");
        if (!IsValueType(loaded, declaration.ValueType, false)) {
            throw std::runtime_error("relation value requires a primitive, enum, Position, or ordinary class");
        }
    }
}

bool UsesDurationSyntax(const TSchemaModule& module) {
    for (const auto& value : module.Classes) {
        for (const auto& field : value.Fields) if (UsesDuration(field.Init)) return true;
        for (const auto& method : value.Methods) if (AnyNode(method.Body, IsDurationCall)) return true;
    }
    for (const auto& value : module.Variants) for (const auto& alternative : value.Alternatives) {
        for (const auto& field : alternative.Fields) if (UsesDuration(field.Init)) return true;
    }
    for (const auto& value : module.Abilities) {
        if (AnyNode(value.Body, IsDurationCall)) return true;
        for (const auto& hook : value.Hooks) if (AnyNode(hook.Body, IsDurationCall)) return true;
    }
    for (const auto& value : module.Activities) if (AnyNode(value.Body, IsDurationCall)) return true;
    for (const auto& value : module.Instances) {
        if (AnyNode(value.Value, IsDurationCall) || AnyNode(value.Requirements, IsDurationCall)) return true;
        for (const auto& parameter : value.Parameters) {
            if (UsesDuration(parameter.Field.Init) || UsesDuration(parameter.Pool) || UsesDuration(parameter.When)) return true;
        }
    }
    return false;
}
