#include <ttrpg/module.h>

#include <ttrpg/parser.h>

#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace fs = std::filesystem;

TSchemaModule ParseFile(const fs::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open schema: " + path.string());
    }
    std::stringstream buf;
    buf << in.rdbuf();
    TParser parser(Tokenize(buf.str()));
    return parser.Parse();
}

void RegisterModuleSymbols(const std::string& stem,
                           const TSchemaModule& mod,
                           std::unordered_map<std::string, TTypeInfo>& table)
{
    auto registerType = [&](const std::string& name, ETypeKind kind) {
        auto it = table.find(name);
        if (it != table.end()) {
            throw std::runtime_error(
                "duplicate type '" + name + "' declared in '" + stem +
                ".ttrpg' (already declared in '" + it->second.OwnerStem + ".ttrpg')");
        }
        table.insert({name, {stem, kind}});
    };
    for (const auto& e : mod.Enums) {
        registerType(e.Name, ETypeKind::Enum);
    }
    for (const auto& v : mod.Variants) {
        registerType(v.Name, ETypeKind::Variant);
    }
    for (const auto& c : mod.Classes) {
        registerType(c.Name, ETypeKind::Class);
    }
}

TLoadedSchemas LoadAll(const fs::path& primaryPath) {
    TLoadedSchemas loaded;
    loaded.PrimaryStem = fs::weakly_canonical(primaryPath).stem().string();

    std::unordered_set<std::string> loading;
    std::unordered_map<std::string, fs::path> pathsByStem;
    std::unordered_map<std::string, std::string> eventOwners;

    auto LoadRec = [&](auto& self, const fs::path& path) -> void {
        fs::path canon = fs::weakly_canonical(path);
        std::string canonStr = canon.string();
        std::string stem = canon.stem().string();

        if (loading.count(canonStr)) {
            throw std::runtime_error("circular import detected: " + canonStr);
        }
        if (auto previous = pathsByStem.find(stem); previous != pathsByStem.end()) {
            if (previous->second != canon) {
                throw std::runtime_error(
                    "module stem '" + stem + "' is shared by '" +
                    previous->second.string() + "' and '" + canonStr + "'");
            }
            if (loaded.ByStem.count(stem)) {
                return;
            }
        }
        pathsByStem.emplace(stem, canon);
        loading.insert(canonStr);

        TSchemaModule mod = ParseFile(canon);

        for (const std::string& imp : mod.Imports) {
            fs::path importPath = canon.parent_path() / imp;
            self(self, importPath);
            loaded.ImportedStems[stem].push_back(fs::weakly_canonical(importPath).stem().string());
        }

        RegisterModuleSymbols(stem, mod, loaded.SymbolTable);
        for (const auto& definition : mod.Abilities) {
            if (!loaded.Definitions.emplace(definition.Name, TAbilityInfo{stem, definition.Kind}).second) {
                throw std::runtime_error("duplicate ability definition '" + definition.Name + "'");
            }
        }
        for (const auto& event : mod.Events) {
            if (!eventOwners.emplace(event.Name, stem).second) {
                throw std::runtime_error("duplicate event definition '" + event.Name + "'");
            }
        }
        loaded.ByStem.emplace(stem, std::move(mod));
        loaded.SourcePaths.push_back(canon);
        loading.erase(canonStr);
    };

    LoadRec(LoadRec, primaryPath);
    loaded.Primary = loaded.ByStem.at(loaded.PrimaryStem);
    return loaded;
}

std::vector<std::string> ImportedModuleStems(const TLoadedSchemas& loaded,
                                           const std::string& ownerStem)
{
    if (const auto found = loaded.ImportedStems.find(ownerStem); found != loaded.ImportedStems.end()) {
        return found->second;
    }
    const auto& module = ownerStem == loaded.PrimaryStem ? loaded.Primary : loaded.ByStem.at(ownerStem);
    std::vector<std::string> result;
    for (const auto& imported : module.Imports) {
        result.push_back(fs::path(imported).stem().string());
    }
    return result;
}

std::map<std::string, const TSchemaModule*> AllModules(const TLoadedSchemas& loaded) {
    std::map<std::string, const TSchemaModule*> result;
    for (const auto& [stem, module] : loaded.ByStem) result.emplace(stem, &module);
    result[loaded.PrimaryStem] = &loaded.Primary;
    return result;
}

const TClassDecl* FindClass(const TLoadedSchemas& loaded, const std::string& name) {
    for (const auto& [stem, module] : AllModules(loaded)) {
        for (const auto& declaration : module->Classes) if (declaration.Name == name) return &declaration;
    }
    return nullptr;
}

const TVariantDecl* FindVariant(const TLoadedSchemas& loaded, const std::string& name) {
    for (const auto& [stem, module] : AllModules(loaded)) {
        for (const auto& declaration : module->Variants) if (declaration.Name == name) return &declaration;
    }
    return nullptr;
}

bool IsGameEntity(const TLoadedSchemas& loaded, const std::string& name) {
    const auto* declaration = FindClass(loaded, name);
    return declaration != nullptr && declaration->GameEntity;
}

bool IsImportedBy(const TLoadedSchemas& loaded, const std::string& callerStem, const std::string& ownerStem) {
    std::set<std::string> visible;
    std::function<void(const std::string&)> visit = [&](const std::string& stem) {
        if (!visible.insert(stem).second) return;
        for (const auto& imported : ImportedModuleStems(loaded, stem)) visit(imported);
    };
    visit(callerStem);
    return visible.contains(ownerStem);
}

bool AnyNode(const expr::TExprNode& node, const FNodePredicate& predicate) {
    if (predicate(node)) return true;
    if (node.Lhs && AnyNode(*node.Lhs, predicate)) return true;
    if (node.Rhs && AnyNode(*node.Rhs, predicate)) return true;
    for (const auto& argument : node.Args) if (AnyNode(argument, predicate)) return true;
    for (const auto& field : node.Fields) if (AnyNode(field.Value, predicate)) return true;
    return false;
}

bool AnyNode(const std::vector<TClassDecl::TStatement>& body, const FNodePredicate& predicate) {
    for (const auto& statement : body) {
        for (const auto* expression : {&statement.Value, &statement.Target, &statement.Candidates}) {
            if (*expression && AnyNode(**expression, predicate)) return true;
        }
        if (AnyNode(statement.Body, predicate) || AnyNode(statement.ElseBody, predicate)) return true;
    }
    return false;
}
