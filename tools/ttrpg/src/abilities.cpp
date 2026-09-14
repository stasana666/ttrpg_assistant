#include <ttrpg/abilities.h>

#include <ttrpg/conventions.h>

#include <algorithm>
#include <cctype>
#include <functional>
#include <stdexcept>
#include <unordered_set>

namespace {

std::vector<std::pair<std::string, const TSchemaModule*>> Modules(const TLoadedSchemas& loaded) {
    std::vector<std::pair<std::string, const TSchemaModule*>> modules;
    for (const auto& [stem, module] : loaded.ByStem) {
        if (stem != loaded.PrimaryStem) modules.emplace_back(stem, &module);
    }
    modules.emplace_back(loaded.PrimaryStem, &loaded.Primary);
    std::sort(modules.begin(), modules.end(), [](const auto& left, const auto& right) {
        return left.first < right.first;
    });
    return modules;
}

[[noreturn]] void GrantError(const TGrantDecl& grant, const std::string& message) {
    throw std::runtime_error(message + " at line " + std::to_string(grant.Location.line) +
        ", col " + std::to_string(grant.Location.col));
}

std::vector<std::string> MemberPath(const expr::TExprNode& value, const TGrantDecl& grant) {
    if (value.Kind == expr::ENodeKind::ContextVar && value.Text == "self") return {};
    if (value.Kind != expr::ENodeKind::Member || !value.Lhs) {
        GrantError(grant, "dynamic grant requires a stored member path rooted at @self");
    }
    auto result = MemberPath(*value.Lhs, grant);
    result.push_back(value.Text);
    return result;
}

}

std::unordered_map<std::string, TAbilityInfo> AbilitySymbols(const TLoadedSchemas& loaded) {
    std::unordered_map<std::string, TAbilityInfo> result;
    for (const auto& [stem, module] : Modules(loaded)) {
        for (const auto& definition : module->Abilities) {
            if (!result.emplace(definition.Name, TAbilityInfo{stem, definition.Kind}).second) {
                throw std::runtime_error("duplicate ability definition '" + definition.Name + "'");
            }
        }
    }
    return result;
}

TAbilityInfo ResolveAbility(const std::string& name, const TLoadedSchemas& loaded) {
    const auto definitions = AbilitySymbols(loaded);
    const auto found = definitions.find(name);
    if (found == definitions.end()) throw std::runtime_error("unknown ability definition '" + name + "'");
    return found->second;
}

std::string AbilityId(const std::string& name, const TLoadedSchemas& loaded) {
    return ResolveAbility(name, loaded).OwnerStem + "." + name;
}

std::string AbilityRegistrationFunction(const std::string& stem) {
    std::string result = "Register";
    bool uppercase = true;
    for (unsigned char character : stem) {
        if (!std::isalnum(character)) {
            uppercase = true;
            continue;
        }
        result += uppercase ? static_cast<char>(std::toupper(character)) : static_cast<char>(character);
        uppercase = false;
    }
    if (result == "Register") throw std::runtime_error("module stem has no registration identifier: '" + stem + "'");
    return result + "Abilities";
}

void ValidateAbilityDefinitions(const TLoadedSchemas& loaded) {
    if (loaded.SymbolTable.contains("Feature")) {
        throw std::runtime_error("type name 'Feature' is reserved for feature selections");
    }
    const auto symbols = AbilitySymbols(loaded);
    std::unordered_map<std::string, const TAbilityDecl*> declarations;
    std::unordered_set<std::string> functions;
    for (const auto& [stem, module] : Modules(loaded)) {
        for (const auto& function : {AbilityRegistrationFunction(stem), RulesRegistrationFunction(stem)}) {
            if (!functions.insert(function).second || loaded.SymbolTable.contains(function)) {
                throw std::runtime_error("ability registration name collision '" + function + "'");
            }
        }
        for (const auto& definition : module->Abilities) {
            if (loaded.SymbolTable.contains(definition.Name) || definition.Name == "Feature") {
                throw std::runtime_error("ability definition conflicts with type '" + definition.Name + "'");
            }
            declarations.emplace(definition.Name, &definition);
        }
    }
    std::unordered_map<std::string, int> marks;
    std::function<void(const std::string&)> visit = [&](const std::string& name) {
        if (marks[name] == 2) return;
        if (marks[name] == 1) throw std::runtime_error("cyclic feature grants involving '" + name + "'");
        marks[name] = 1;
        const auto& definition = *declarations.at(name);
        std::unordered_set<std::string> seen;
        for (const auto& grant : definition.Grants) {
            const auto& value = grant.Source;
            if (value.Kind != expr::ENodeKind::Var || value.HasDollar) {
                GrantError(grant, "feature grants must name an action, reaction, or feature definition");
            }
            if (!symbols.contains(value.Text)) GrantError(grant, "unknown ability definition '" + value.Text + "'");
            if (!seen.insert(value.Text).second) GrantError(grant, "duplicate feature grant '" + value.Text + "'");
            visit(value.Text);
        }
        marks[name] = 2;
    };
    for (const auto& [stem, module] : Modules(loaded)) {
        for (const auto& definition : module->Abilities) visit(definition.Name);
    }
}

std::string RulesRegistrationFunction(const std::string& stem) {
    auto name = AbilityRegistrationFunction(stem);
    name.resize(name.size() - std::string("Abilities").size());
    return name + "Rules";
}

std::vector<TCompiledGrant> CompileGrants(const TClassDecl& declaration, const TLoadedSchemas& loaded) {
    std::unordered_map<std::string, const TClassDecl*> classes;
    for (const auto& [stem, module] : Modules(loaded)) {
        for (const auto& value : module->Classes) classes[value.Name] = &value;
    }
    classes[declaration.Name] = &declaration;
    std::vector<TCompiledGrant> result;
    std::unordered_set<std::string> sites;
    for (const auto& grant : declaration.Grants) {
        TCompiledGrant compiled;
        if (grant.Source.Kind == expr::ENodeKind::Var && !grant.Source.HasDollar) {
            compiled.DefinitionId = AbilityId(grant.Source.Text, loaded);
            compiled.Site = declaration.Name + ".grant." + compiled.DefinitionId;
        } else {
            const auto fields = MemberPath(grant.Source, grant);
            if (fields.empty()) GrantError(grant, "grant must select a stored field");
            const TClassDecl* current = &declaration;
            const TFieldDecl* selected = nullptr;
            compiled.Accessor = "(*this)";
            for (std::size_t i = 0; i < fields.size(); ++i) {
                const auto found = std::find_if(current->Fields.begin(), current->Fields.end(), [&](const auto& field) {
                    return field.Name == fields[i];
                });
                if (found == current->Fields.end()) GrantError(grant, "unknown grant field '" + fields[i] + "'");
                selected = &*found;
                if (selected->Derived) GrantError(grant, "derived values cannot provide a stable grant source");
                compiled.Fields.push_back(PascalToSnake(selected->Name));
                compiled.Accessor += "." + selected->Name + "()";
                if (i + 1 < fields.size()) {
                    const auto next = classes.find(selected->TypeName);
                    if (selected->Container != EContainer::None || next == classes.end()) {
                        GrantError(grant, "grant member path must traverse stored class fields");
                    }
                    current = next->second;
                }
            }
            if (selected->Container != EContainer::None && selected->Container != EContainer::Collection) {
                GrantError(grant, "grant sources support scalar values or collections");
            }
            const auto provider = classes.find(selected->TypeName);
            if (!IsBuiltinFeature(selected->TypeName) &&
                (provider == classes.end() || provider->second->Grants.empty())) {
                GrantError(grant, "grant field must contain Feature values or classes with grant declarations");
            }
            compiled.Kind = selected->Container == EContainer::Collection ?
                TCompiledGrant::EKind::Collection : TCompiledGrant::EKind::Value;
            compiled.Site = declaration.Name + ".grant";
            for (const auto& field : compiled.Fields) compiled.Site += "." + field;
        }
        if (!sites.insert(compiled.Site).second) GrantError(grant, "duplicate grant source");
        result.push_back(std::move(compiled));
    }
    return result;
}
