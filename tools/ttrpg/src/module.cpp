#include <ttrpg/module.h>

#include <ttrpg/parser.h>

#include <fstream>
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

// Recursively load primary + all transitive imports.
// Detects cycles via the "loading" set.
TLoadedSchemas LoadAll(const fs::path& primaryPath) {
    TLoadedSchemas loaded;
    loaded.PrimaryStem = primaryPath.stem().string();

    std::unordered_set<std::string> loading;

    auto LoadRec = [&](auto& self, const fs::path& path) -> void {
        fs::path canon = fs::weakly_canonical(path);
        std::string canonStr = canon.string();
        std::string stem = canon.stem().string();

        if (loading.count(canonStr)) {
            throw std::runtime_error("circular import detected: " + canonStr);
        }
        if (loaded.ByStem.count(stem)) {
            return;  // already loaded
        }
        loading.insert(canonStr);

        TSchemaModule mod = ParseFile(canon);

        for (const std::string& imp : mod.Imports) {
            fs::path importPath = canon.parent_path() / imp;
            self(self, importPath);
        }

        RegisterModuleSymbols(stem, mod, loaded.SymbolTable);
        loaded.ByStem.emplace(stem, std::move(mod));
        loading.erase(canonStr);
    };

    LoadRec(LoadRec, primaryPath);
    loaded.Primary = loaded.ByStem.at(loaded.PrimaryStem);
    return loaded;
}
