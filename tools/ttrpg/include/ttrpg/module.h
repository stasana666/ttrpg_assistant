#pragma once


#include <ttrpg/schema_ast.h>

#include <filesystem>
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
