#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>

#include <compare>
#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

enum class EAbilityKind {
    Action,
    Reaction,
    Feature,
};

struct TAbilityDefinition {
    std::string Id;
    EAbilityKind Kind;
    std::vector<std::string> Grants;

    friend bool operator==(const TAbilityDefinition&, const TAbilityDefinition&) = default;
};

class TAbilityCatalog {
public:
    void Register(TAbilityDefinition definition);
    const TAbilityDefinition* Find(std::string_view id) const;
    const TAbilityDefinition& Require(std::string_view id) const;

private:
    std::map<std::string, TAbilityDefinition, std::less<>> definitions_;
};

class TFeatureSelection {
public:
    TFeatureSelection() = default;
    explicit TFeatureSelection(std::string id);

    const std::string& DefinitionId() const;
    TAstNode GetAst(TAstContext& ctx) const;

    friend bool operator==(const TFeatureSelection&, const TFeatureSelection&) = default;

private:
    std::string definition_id_;
};

class TAbilitySourcePath {
public:
    TAbilitySourcePath Field(std::string name) const;
    TAbilitySourcePath Entry(std::size_t id) const;
    bool StartsWith(const TAbilitySourcePath& prefix) const;
    std::string ToString() const;

    friend auto operator<=>(const TAbilitySourcePath&, const TAbilitySourcePath&) = default;

private:
    std::vector<std::variant<std::string, std::size_t>> parts_;
};

std::string AstSerialize(const TAbilitySourcePath& path);

struct TGrantedAbility {
    std::string DefinitionId;
    EAbilityKind Kind;
    TAbilitySourcePath Source;
    std::vector<std::string> Provenance;

    friend bool operator==(const TGrantedAbility&, const TGrantedAbility&) = default;
};

class TAbilityCollector {
public:
    explicit TAbilityCollector(const TAbilityCatalog& catalog);

    void Grant(std::string_view definition, const TAbilitySourcePath& path,
               std::string_view site);
    void Visit(const TFeatureSelection& selection, const TAbilitySourcePath& path,
               std::string_view site);

    template <class T>
        requires requires(const T& value, TAbilityCollector& collector,
                          const TAbilitySourcePath& path) {
            value.VisitGrantedAbilities(collector, path);
        }
    void Visit(const T& value, const TAbilitySourcePath& path, std::string_view site)
    {
        TAbilityCollector nested(*catalog_);
        nested.sites_ = sites_;
        nested.sites_.emplace_back(site);
        value.VisitGrantedAbilities(nested, path);
        Merge(nested.grants_, nested.features_);
    }

    const std::vector<TGrantedAbility>& Grants() const;
    const std::vector<TGrantedAbility>& Features() const;
    const TGrantedAbility* FindGrant(std::string_view definition, const TAbilitySourcePath& source) const;

private:
    void Expand(std::string_view definition, const TAbilitySourcePath& path,
                std::vector<std::string>& chain, const std::vector<std::string>& sites,
                std::vector<TGrantedAbility>& output,
                std::vector<TGrantedAbility>& features) const;
    void Merge(const std::vector<TGrantedAbility>& grants,
               const std::vector<TGrantedAbility>& features);

    const TAbilityCatalog* catalog_;
    std::vector<std::string> sites_;
    std::vector<TGrantedAbility> grants_;
    std::vector<TGrantedAbility> features_;
};

template <class T>
TAbilityCollector CollectAbilities(const T& value, const TAbilityCatalog& catalog)
{
    TAbilityCollector collector(catalog);
    value.VisitGrantedAbilities(collector);
    return collector;
}

template <>
struct TIsAstRecursive<TFeatureSelection> : std::true_type {};
