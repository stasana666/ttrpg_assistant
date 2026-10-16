#include <ttrpg/records.h>

#include <ttrpg/conventions.h>

#include <set>
#include <stdexcept>

namespace {

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

TDeclarations<TRecordDecl> RecordSymbols(const TLoadedSchemas& loaded) {
    return CollectDeclarations(loaded, &TSchemaModule::Records, "record");
}

TRecordInfo ResolveRecord(const std::string& name, const std::string& callerStem, const TLoadedSchemas& loaded) {
    return ResolveDeclaration(loaded, &TSchemaModule::Records, "record", name, callerStem);
}

std::string RecordTagName(const TRecordDecl& declaration) {
    return "TRecord" + declaration.Name;
}

void ValidateRecordDefinitions(const TLoadedSchemas& loaded) {
    const std::set<std::string> reserved{"TOwner", "TSource", "TArguments", "TStored", "TInputs", "kId"};
    for (const auto& [name, info] : RecordSymbols(loaded)) {
        const auto& declaration = *info.Declaration;
        if (!IsGameEntity(loaded, declaration.OwnerType)) throw std::runtime_error("record owner must be a GameEntity");
        const auto* source = FindClass(loaded, declaration.SourceType);
        if (!source || source->GameEntity) throw std::runtime_error("record source must be an ordinary class");
        if (reserved.contains(declaration.SourceName)) throw std::runtime_error("reserved record source name '" + declaration.SourceName + "'");
        std::set<std::string> parameters{declaration.SourceName};
        for (const auto& parameter : declaration.Parameters) {
            if (!IsValueType(loaded, parameter.TypeName, true)) throw std::runtime_error("unsupported record parameter type '" + parameter.TypeName + "'");
            if (reserved.contains(parameter.Name) || !parameters.insert(parameter.Name).second) throw std::runtime_error("duplicate or reserved record parameter '" + parameter.Name + "'");
        }
    }
    for (const auto& [stem, module] : AllModules(loaded)) {
        for (const auto& value : module->Activities) CheckPublication(value.Body, false);
        for (const auto& value : module->Classes) for (const auto& method : value.Methods) CheckPublication(method.Body, false);
        for (const auto& ability : module->Abilities) {
            CheckPublication(ability.Body, ability.Kind == EAbilityDeclarationKind::Action && !ability.OwnerType.empty());
            for (const auto& hook : ability.Hooks) CheckPublication(hook.Body, false);
            if (ability.PreviousRecord.empty()) continue;
            if (ability.Kind != EAbilityDeclarationKind::Action || ability.OwnerType.empty()) throw std::runtime_error("after requires a root action body");
            const auto& record = *ResolveRecord(ability.PreviousRecord, stem, loaded).Declaration;
            if (record.OwnerType != ability.OwnerType) throw std::runtime_error("after record owner does not match action owner");
            if (ability.PreviousName.empty() || ability.PreviousName == ability.SourceName) throw std::runtime_error("after binding conflicts with action source");
        }
    }
}
