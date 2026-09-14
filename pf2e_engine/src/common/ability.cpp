#include <pf2e_engine/common/ability.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

void TAbilityCatalog::Register(TAbilityDefinition definition)
{
    if (definition.Id.empty()) {
        throw std::invalid_argument("ability definition id must not be empty");
    }
    if (definition.Kind != EAbilityKind::Action && definition.Kind != EAbilityKind::Reaction
        && definition.Kind != EAbilityKind::Feature) {
        throw std::invalid_argument("invalid ability kind");
    }
    if (definition.Kind != EAbilityKind::Feature && !definition.Grants.empty()) {
        throw std::invalid_argument("only features may grant other abilities");
    }
    for (const auto& grant : definition.Grants) {
        if (grant.empty()) {
            throw std::invalid_argument("granted definition id must not be empty");
        }
    }
    const auto found = definitions_.find(definition.Id);
    if (found != definitions_.end()) {
        if (found->second != definition) {
            throw std::invalid_argument("conflicting ability definition: " + definition.Id);
        }
        return;
    }
    const std::string id = definition.Id;
    definitions_.emplace(id, std::move(definition));
}

const TAbilityDefinition* TAbilityCatalog::Find(std::string_view id) const
{
    const auto found = definitions_.find(id);
    return found == definitions_.end() ? nullptr : &found->second;
}

const TAbilityDefinition& TAbilityCatalog::Require(std::string_view id) const
{
    const auto* found = Find(id);
    if (found == nullptr) {
        throw std::invalid_argument("unknown ability definition: " + std::string(id));
    }
    return *found;
}

TFeatureSelection::TFeatureSelection(std::string id)
    : definition_id_(std::move(id))
{
    if (definition_id_.empty()) {
        throw std::invalid_argument("feature selection id must not be empty");
    }
}

const std::string& TFeatureSelection::DefinitionId() const
{
    return definition_id_;
}

nlohmann::json TFeatureSelection::ToJson() const
{
    if (definition_id_.empty()) {
        return nullptr;
    }
    return definition_id_;
}

TFeatureSelection TFeatureSelection::FromJson(const nlohmann::json& json)
{
    if (json.is_null()) {
        return {};
    }
    if (!json.is_string()) {
        throw std::invalid_argument("feature selection must be a definition id or null");
    }
    return TFeatureSelection(json.get<std::string>());
}

TAstNode TFeatureSelection::GetAst([[maybe_unused]] TAstContext& ctx) const
{
    AST_ASSERT_LAYOUT(TFeatureSelection, 32);
    TAstNode node = TAstNode::MakeObject("TFeatureSelection");
    AddValueField(node, "definition_id", definition_id_);
    return node;
}

TAbilitySourcePath TAbilitySourcePath::Field(std::string name) const
{
    if (name.empty()) {
        throw std::invalid_argument("ability source field must not be empty");
    }
    TAbilitySourcePath path = *this;
    path.parts_.emplace_back(std::move(name));
    return path;
}

TAbilitySourcePath TAbilitySourcePath::Entry(std::size_t id) const
{
    TAbilitySourcePath path = *this;
    path.parts_.emplace_back(id);
    return path;
}

bool TAbilitySourcePath::StartsWith(const TAbilitySourcePath& prefix) const
{
    return parts_.size() >= prefix.parts_.size()
        && std::equal(prefix.parts_.begin(), prefix.parts_.end(), parts_.begin());
}

std::string TAbilitySourcePath::ToString() const
{
    std::string result = "$";
    for (const auto& part : parts_) {
        result += '[';
        if (const auto* field = std::get_if<std::string>(&part)) {
            result += nlohmann::json(*field).dump();
        } else {
            result += std::to_string(std::get<std::size_t>(part));
        }
        result += ']';
    }
    return result;
}

std::string AstSerialize(const TAbilitySourcePath& path)
{
    return "string:" + path.ToString();
}

TAbilityCollector::TAbilityCollector(const TAbilityCatalog& catalog)
    : catalog_(&catalog)
{
}

void TAbilityCollector::Grant(std::string_view definition, const TAbilitySourcePath& path,
                              std::string_view site)
{
    if (site.empty()) {
        throw std::invalid_argument("ability grant site must not be empty");
    }
    auto sites = sites_;
    sites.emplace_back(site);
    std::vector<std::string> chain;
    std::vector<TGrantedAbility> expanded;
    std::vector<TGrantedAbility> features;
    Expand(definition, path, chain, sites, expanded, features);
    Merge(expanded, features);
}

void TAbilityCollector::Visit(const TFeatureSelection& selection,
                              const TAbilitySourcePath& path, std::string_view site)
{
    if (!selection.DefinitionId().empty()) {
        if (catalog_->Require(selection.DefinitionId()).Kind != EAbilityKind::Feature) {
            throw std::invalid_argument("feature selection must refer to a feature");
        }
        Grant(selection.DefinitionId(), path, site);
    }
}

const std::vector<TGrantedAbility>& TAbilityCollector::Grants() const
{
    return grants_;
}

const std::vector<TGrantedAbility>& TAbilityCollector::Features() const
{
    return features_;
}

void TAbilityCollector::Expand(std::string_view definition, const TAbilitySourcePath& path,
                               std::vector<std::string>& chain,
                               const std::vector<std::string>& sites,
                               std::vector<TGrantedAbility>& output,
                               std::vector<TGrantedAbility>& features) const
{
    if (std::find(chain.begin(), chain.end(), definition) != chain.end()) {
        throw std::invalid_argument("cyclic ability grant: " + std::string(definition));
    }
    const auto* found = catalog_->Find(definition);
    if (found == nullptr) {
        throw std::invalid_argument("unknown granted ability: " + std::string(definition));
    }
    chain.emplace_back(definition);
    auto& destination = found->Kind == EAbilityKind::Feature ? features : output;
    destination.push_back(TGrantedAbility{
        .DefinitionId = found->Id,
        .Kind = found->Kind,
        .Source = path,
        .Provenance = {nlohmann::json{{"sites", sites}, {"definitions", chain}}.dump()},
    });
    for (const auto& grant : found->Grants) {
        Expand(grant, path, chain, sites, output, features);
    }
    chain.pop_back();
}

namespace {

void MergeGrants(std::vector<TGrantedAbility>& merged, const std::vector<TGrantedAbility>& grants)
{
    for (const auto& grant : grants) {
        auto found = std::find_if(merged.begin(), merged.end(), [&](const auto& existing) {
            return existing.DefinitionId == grant.DefinitionId && existing.Source == grant.Source;
        });
        if (found == merged.end()) {
            merged.push_back(grant);
            continue;
        }
        for (const auto& provenance : grant.Provenance) {
            if (std::find(found->Provenance.begin(), found->Provenance.end(), provenance)
                == found->Provenance.end()) {
                found->Provenance.push_back(provenance);
            }
        }
    }
}

}

void TAbilityCollector::Merge(const std::vector<TGrantedAbility>& grants,
                              const std::vector<TGrantedAbility>& features)
{
    auto merged_grants = grants_;
    auto merged_features = features_;
    MergeGrants(merged_grants, grants);
    MergeGrants(merged_features, features);
    grants_.swap(merged_grants);
    features_.swap(merged_features);
}
