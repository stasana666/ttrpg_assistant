#pragma once

#include <ttrpg/schema_ast.h>

#include <filesystem>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

TSchemaModule ParseFile(const std::filesystem::path& path);

enum class ETypeKind {
    Enum,
    Class,
    Variant,
};

struct TTypeInfo {
    std::string OwnerStem;
    ETypeKind Kind = ETypeKind::Class;
};

struct TAbilityInfo {
    std::string OwnerStem;
    EAbilityDeclarationKind Kind;
};

struct TLoadedSchemas {
    TSchemaModule Primary;
    std::string PrimaryStem;
    std::unordered_map<std::string, TSchemaModule> ByStem;
    std::unordered_map<std::string, TTypeInfo> SymbolTable;
    std::vector<std::filesystem::path> SourcePaths;
    std::unordered_map<std::string, TAbilityInfo> Definitions;
    std::unordered_map<std::string, std::vector<std::string>> ImportedStems;
};

void RegisterModuleSymbols(const std::string& stem,
                           const TSchemaModule& mod,
                           std::unordered_map<std::string, TTypeInfo>& table);

TLoadedSchemas LoadAll(const std::filesystem::path& primaryPath);
std::vector<std::string> ImportedModuleStems(const TLoadedSchemas& loaded,
                                           const std::string& ownerStem);

std::map<std::string, const TSchemaModule*> AllModules(const TLoadedSchemas& loaded);
const TClassDecl* FindClass(const TLoadedSchemas& loaded, const std::string& name);
const TVariantDecl* FindVariant(const TLoadedSchemas& loaded, const std::string& name);
bool IsGameEntity(const TLoadedSchemas& loaded, const std::string& name);
bool IsImportedBy(const TLoadedSchemas& loaded, const std::string& callerStem, const std::string& ownerStem);

using FNodePredicate = std::function<bool(const expr::TExprNode&)>;
bool AnyNode(const expr::TExprNode& node, const FNodePredicate& predicate);
bool AnyNode(const std::vector<TClassDecl::TStatement>& body, const FNodePredicate& predicate);

template <class TDecl>
struct TDeclarationInfo {
    std::string OwnerStem;
    const TDecl* Declaration = nullptr;
};

template <class TDecl>
using TDeclarations = std::unordered_map<std::string, TDeclarationInfo<TDecl>>;

template <class TDecl>
TDeclarations<TDecl> CollectDeclarations(const TLoadedSchemas& loaded, std::vector<TDecl> TSchemaModule::*member,
                                         const std::string& kind) {
    TDeclarations<TDecl> result;
    for (const auto& [stem, module] : AllModules(loaded)) {
        for (const auto& declaration : module->*member) {
            if (!result.emplace(declaration.Name, TDeclarationInfo<TDecl>{stem, &declaration}).second) {
                throw std::runtime_error("duplicate " + kind + " '" + declaration.Name + "'");
            }
        }
    }
    return result;
}

template <class TDecl>
TDeclarationInfo<TDecl> ResolveDeclaration(const TLoadedSchemas& loaded, std::vector<TDecl> TSchemaModule::*member,
                                           const std::string& kind, const std::string& name,
                                           const std::string& callerStem) {
    const auto declarations = CollectDeclarations(loaded, member, kind);
    const auto found = declarations.find(name);
    if (found == declarations.end()) throw std::runtime_error("unknown " + kind + " '" + name + "'");
    if (!IsImportedBy(loaded, callerStem, found->second.OwnerStem)) {
        throw std::runtime_error(kind + " '" + name + "' is not imported by '" + callerStem + "'");
    }
    return found->second;
}
