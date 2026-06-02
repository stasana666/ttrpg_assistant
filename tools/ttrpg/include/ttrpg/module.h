#pragma once

// Module loading + cross-file symbol table. Loads a primary `.ttrpg` plus all
// transitive imports (cycle-detected) and records every declared type's kind
// and owning file.

#include <ttrpg/schema_ast.h>

#include <filesystem>
#include <string>
#include <unordered_map>

TSchemaModule ParseFile(const std::filesystem::path& path);

enum class ETypeKind {
    Enum,
    Class,
    Variant,
};

struct TTypeInfo {
    std::string OwnerStem;   // filename stem of the .ttrpg that declares this type
    ETypeKind Kind = ETypeKind::Class;
};

struct TLoadedSchemas {
    TSchemaModule Primary;
    std::string PrimaryStem;
    // Stem -> module, for all transitively-loaded files (including primary).
    std::unordered_map<std::string, TSchemaModule> ByStem;
    // Type name -> (owner_stem, kind). Built across all loaded modules.
    std::unordered_map<std::string, TTypeInfo> SymbolTable;
};

void RegisterModuleSymbols(const std::string& stem,
                           const TSchemaModule& mod,
                           std::unordered_map<std::string, TTypeInfo>& table);

TLoadedSchemas LoadAll(const std::filesystem::path& primaryPath);
