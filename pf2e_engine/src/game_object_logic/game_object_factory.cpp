#include <game_object_factory.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>

#include <cpp_config.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <limits>

#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/inventory/abilities.h>
#include <pf2e_engine/inventory/flanking.h>
#include <pf2e_engine/inventory/movement.h>
#include <pf2e_engine/inventory/shield_actions.h>
#include <pf2e_engine/inventory/combat_timing.h>
#include <pf2e_engine/inventory/intimidation.h>
#include <pf2e_engine/inventory/trip.h>
#include <pf2e_engine/inventory/remaster_profiles.h>
#include <pf2e_engine/inventory/material.h>
#include "battle_map.h"
#include "characteristics.h"
#include "game_object_id.h"
#include "proficiency.h"
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/creature.h>

#include <pf2e_engine/actions/action_reader.h>
#include <pf2e_engine/feat.h>
#include <pf2e_engine/player.h>

namespace {

struct TConstructionTraceEntry {
    const TGameObjectFactory* factory;
    std::type_index type;
    TGameObjectId id;
};

thread_local std::vector<TConstructionTraceEntry> generated_constructions;

nlohmann::json NormalizeLegacyProficiencyValue(const nlohmann::json& value)
{
    if (value.is_number_unsigned()) {
        if (value.get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            throw std::invalid_argument("proficiency override exceeds int range");
        }
        return {{"is_fixed", true}, {"fixed_bonus", value.get<int>()}};
    }
    if (value.is_number_integer()) {
        const auto bonus = value.get<std::int64_t>();
        if (bonus < std::numeric_limits<int>::min() || bonus > std::numeric_limits<int>::max()) {
            throw std::invalid_argument("proficiency override exceeds int range");
        }
        return {{"is_fixed", true}, {"fixed_bonus", static_cast<int>(bonus)}};
    }
    if (!value.is_string()) {
        throw std::invalid_argument("legacy proficiency must be a rank or an integer bonus");
    }
    auto rank = value.get<std::string>();
    if (rank.empty()) {
        throw std::invalid_argument("proficiency rank must not be empty");
    }
    rank.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(rank.front())));
    EProficiencyRankFromString(rank);
    return {{"rank", rank}};
}

nlohmann::json NormalizeLegacyProficiency(const nlohmann::json& value)
{
    if (!value.is_object()) {
        throw std::invalid_argument("proficiency must be an object");
    }
    nlohmann::json result = nlohmann::json::object();
    for (const auto* field : {"armor_category", "weapon_category", "savethrow", "skill"}) {
        if (!value.contains(field)) {
            continue;
        }
        const auto& entries = value.at(field);
        if (!entries.is_object()) {
            throw std::invalid_argument("proficiency categories must be objects");
        }
        result[field] = nlohmann::json::object();
        for (const auto& [key, entry] : entries.items()) {
            result[field][key] = NormalizeLegacyProficiencyValue(entry);
        }
    }
    if (value.contains("Perception")) {
        result["perception"] = NormalizeLegacyProficiencyValue(value.at("Perception"));
    }
    return result;
}

}

TGameObjectFactory::TGeneratedConstructionGuard::TGeneratedConstructionGuard(
    const TGameObjectFactory& factory, std::type_index type, TGameObjectId id)
{
    auto cycle = std::find_if(generated_constructions.begin(), generated_constructions.end(),
        [&](const TConstructionTraceEntry& entry) {
            return entry.factory == &factory && entry.type == type && entry.id == id;
        });
    if (cycle != generated_constructions.end()) {
        std::string message = "cyclic generated object reference: ";
        for (auto entry = cycle; entry != generated_constructions.end(); ++entry) {
            if (entry->factory == &factory) {
                message += std::string(TGameObjectIdManager::Instance().Name(entry->id)) + " -> ";
            }
        }
        throw std::runtime_error(message + std::string(TGameObjectIdManager::Instance().Name(id)));
    }
    generated_constructions.push_back({&factory, type, id});
}

TGameObjectFactory::TGeneratedConstructionGuard::~TGeneratedConstructionGuard()
{
    generated_constructions.pop_back();
}

const std::string kPathToSchema = kRootDirPath + "/pf2e_engine/schemas/schema.json";

const std::unordered_map<std::string, TGameObjectFactory::FMethod>
TGameObjectFactory::kReaderMapping = {
    {"pf2e_armor", &TGameObjectFactory::ReadArmor},
    {"pf2e_material", &TGameObjectFactory::ReadMaterial},
    {"pf2e_weapon", &TGameObjectFactory::ReadWeapon},
    {"pf2e_race", &TGameObjectFactory::ReadRace},
    {"pf2e_class", &TGameObjectFactory::ReadClass},
    {"pf2e_ability_score", &TGameObjectFactory::ReadAbilityScore},
    {"pf2e_ability_scores", &TGameObjectFactory::ReadAbilityScores},
    {"pf2e_creature_data", &TGameObjectFactory::ReadCreatureData},
    {"pf2e_creature", &TGameObjectFactory::ReadCreature},
    {"pf2e_action", &TGameObjectFactory::ReadAction},
    {"pf2e_battle_map", &TGameObjectFactory::ReadBattleMap},
};

void TGameObjectFactory::AddSource(const std::filesystem::path& source_path)
{
    if (!std::filesystem::exists(source_path)) {
        std::stringstream ss;
        ss << "File by path \"" << source_path << "\" not found";
        throw std::runtime_error(ss.str());
    }
    if (std::filesystem::is_regular_file(source_path)) {
        return ReadObjectFromFile(source_path);
    }
    if (std::filesystem::is_directory(source_path)) {
        for (auto de : std::filesystem::directory_iterator{source_path}) {
            if (de.is_regular_file()) {
                ReadObjectFromFile(de);
            }
        }
        return;
    }
    throw std::runtime_error("not supported source type: \"" + source_path.string() + "\"");
}

void TGameObjectFactory::ReadObjectFromFile(const std::filesystem::path& game_object_file)
{
    std::ifstream game_object_stream(game_object_file);
    nlohmann::json json_game_object = nlohmann::json::parse(game_object_stream);
    if (ReadGeneratedObject(json_game_object)) {
        return;
    }
    ValidateObject(json_game_object);
    
    auto reader = kReaderMapping.find(json_game_object["type"]);

    if (reader == kReaderMapping.end()) {
        std::stringstream ss;
        ss << "unknown type of game object " << json_game_object["type"];
        throw std::runtime_error(ss.str());
    }

    TGameObjectId id = ReadGameObjectName(json_game_object);
    (this->*reader->second)(json_game_object[json_game_object["type"]], id);
}

void TGameObjectFactory::RegisterGeneratedType(std::type_index type, std::string json_type)
{
    if (json_type.empty() || kReaderMapping.contains(json_type)) {
        throw std::invalid_argument("invalid generated type name: " + json_type);
    }
    for (const auto& [name, registered_type] : generated_types_) {
        if (name == json_type && registered_type == type) {
            return;
        }
        if (name == json_type || registered_type == type) {
            throw std::invalid_argument("generated type registration conflicts with: " + name);
        }
    }
    generated_types_.emplace(std::move(json_type), type);
    generated_definitions_.try_emplace(type);
}

bool TGameObjectFactory::ReadGeneratedObject(const nlohmann::json& json)
{
    if (!json.is_object() || !json.contains("type") || !json.at("type").is_string()) {
        return false;
    }
    const std::string json_type = json.at("type").get<std::string>();
    const auto registered = generated_types_.find(json_type);
    if (registered == generated_types_.end()) {
        return false;
    }
    if (!json.contains("name") || !json.at("name").is_string()
        || !json.contains(json_type) || !json.at(json_type).is_object()) {
        throw std::invalid_argument(
            "generated object requires a string name and an object payload: " + json_type);
    }
    const std::string name = json.at("name").get<std::string>();
    const TGameObjectId id = TGameObjectIdManager::Instance().Register(name);
    auto& definitions = generated_definitions_.at(registered->second);
    if (!definitions.emplace(id, std::make_shared<const nlohmann::json>(json.at(json_type))).second) {
        throw std::invalid_argument("duplicate generated object '" + name + "' of type " + json_type);
    }
    return true;
}

const nlohmann::json& TGameObjectFactory::GeneratedDefinition(std::type_index type, TGameObjectId id) const
{
    auto definitions = generated_definitions_.find(type);
    const std::string name(TGameObjectIdManager::Instance().Name(id));
    if (definitions == generated_definitions_.end()) {
        throw std::runtime_error("no generated type registered for reference: " + name);
    }
    auto definition = definitions->second.find(id);
    if (definition == definitions->second.end()) {
        throw std::runtime_error("unknown generated object reference: " + name);
    }
    return *definition->second;
}

void TGameObjectFactory::ValidateObject(nlohmann::json& json) const
{
    using Validator = nlohmann::json_schema::json_validator;
    static std::unordered_map<std::string, Validator> validators;
    static nlohmann::json schema = []() {
        std::ifstream in(kPathToSchema);
        nlohmann::json schema = nlohmann::json::parse(in);
        return schema;
    }();
    static Validator object_validator{schema};

    object_validator.validate(json);

    Validator& validator = [&]() -> Validator& {
        auto type = json["type"].get<std::string>();
        if (!validators.contains(type)) {
            schema["$ref"] = "#/$defs/" + type;
            validators.emplace(type, schema);
        }
        return validators.find(type)->second;
    }();

    const auto default_patch = validator.validate(json);
	json = json.patch(default_patch);
}

TGameObjectId TGameObjectFactory::ReadGameObjectName(nlohmann::json& json_game_object) const
{
    std::string name = json_game_object["name"];
    return TGameObjectIdManager::Instance().Register(name);
}


void TGameObjectFactory::ReadArmor(nlohmann::json& json_game_object, TGameObjectId id)
{
    armors_.insert({id, [json_game_object](const TGameObjectFactory& factory) {
        return TArmor::FromJson(json_game_object, factory);
    }});
}

void TGameObjectFactory::ReadMaterial(nlohmann::json& json_game_object, TGameObjectId id)
{
    TMaterial result = TMaterial::FromJson(json_game_object, *this);
    materials_.insert({id, [result]([[maybe_unused]] const TGameObjectFactory& factory) { return result; }});
}


void TGameObjectFactory::ReadWeapon(nlohmann::json& json_game_object, TGameObjectId id)
{
    TWeapon result = TWeapon::FromJson(json_game_object, *this);
    weapons_.insert({id, [result]([[maybe_unused]] const TGameObjectFactory& factory) { return result; }});
}


void TGameObjectFactory::ReadRace(nlohmann::json& json_game_object, TGameObjectId id)
{
    TRace result = TRace::FromJson(json_game_object, *this);
    races_.insert({id, [result]([[maybe_unused]] const TGameObjectFactory& factory) { return result; }});
}

void TGameObjectFactory::ReadClass(nlohmann::json& json_game_object, TGameObjectId id)
{
    TClass result = TClass::FromJson(json_game_object, *this);
    classes_.insert({id, [result]([[maybe_unused]] const TGameObjectFactory& factory) { return result; }});
}

void TGameObjectFactory::ReadAbilityScore(nlohmann::json& json_game_object, TGameObjectId id)
{
    TAbilityScore result = TAbilityScore::FromJson(json_game_object, *this);
    ability_score_.insert({id, [result]([[maybe_unused]] const TGameObjectFactory& factory) { return result; }});
}

void TGameObjectFactory::ReadAbilityScores(nlohmann::json& json_game_object, TGameObjectId id)
{
    TAbilityScores result = TAbilityScores::FromJson(json_game_object, *this);
    ability_scores_.insert({id, [result]([[maybe_unused]] const TGameObjectFactory& factory) { return result; }});
}


void TGameObjectFactory::ReadCreatureData(nlohmann::json& json_game_object, TGameObjectId id)
{
    StoreCreature(json_game_object, id);
}

void TGameObjectFactory::StoreCreature(nlohmann::json data, TGameObjectId id)
{
    auto& definitions = generated_definitions_[typeid(TCreature)];
    if (!definitions.emplace(id, std::make_shared<const nlohmann::json>(std::move(data))).second) {
        throw std::invalid_argument("duplicate creature definition: " +
            std::string(TGameObjectIdManager::Instance().Name(id)));
    }
}

void TGameObjectFactory::ValidateCreature(const TCreature& creature) const
{
    const auto rules = AbilityRules();
    const auto collector = CollectAbilities(creature, rules->Catalog);
    for (const auto& grant : collector.Grants()) {
        RequireGrantImplementation(*rules, grant);
    }
}

void TGameObjectFactory::ReadCreature(nlohmann::json& json_game_object, TGameObjectId id)
{
    const auto definitions = generated_definitions_.find(typeid(TCreature));
    if (definitions != generated_definitions_.end() && definitions->second.contains(id)) {
        throw std::invalid_argument("duplicate creature definition: " +
            std::string(TGameObjectIdManager::Instance().Name(id)));
    }
    nlohmann::json data = json_game_object.at("creature_data");
    if (json_game_object.contains("actions")) {
        auto& features = data["features"];
        if (features.is_null()) {
            features = nlohmann::json::array();
        }
        for (const auto& action : json_game_object.at("actions")) {
            features.push_back("legacy_feature." + action.get<std::string>());
        }
    }

    if (json_game_object.contains("proficiency")) {
        if (data.contains("proficiency")) {
            throw std::invalid_argument("proficiency is defined in both creature data and legacy envelope");
        }
        data["proficiency"] = NormalizeLegacyProficiency(json_game_object.at("proficiency"));
    }

    std::map<std::string, std::shared_ptr<const TCreatureFeat>, std::less<>> hooks;
    if (json_game_object.contains("feats")) {
        std::size_t feat_index = 0;
        for (auto& feat_json : json_game_object["feats"]) {
            TPipelineReader reader;
            auto feat = std::make_shared<TCreatureFeat>();
            feat->name = feat_json["name"];
            const auto& block_json = feat_json["block"];
            if (block_json.is_string()) {
                feat->blocks.push_back(block_json.get<std::string>());
            } else {
                for (const auto& name : block_json) {
                    feat->blocks.push_back(name.get<std::string>());
                }
            }
            feat->pipeline = reader.ReadPipeline(feat_json["pipeline"]);
            const std::string feature_id = "legacy_feat." +
                std::string(TGameObjectIdManager::Instance().Name(id)) + "." + std::to_string(feat_index++);
            hooks.emplace(feature_id, std::move(feat));
            if (!data.contains("features")) {
                data["features"] = nlohmann::json::array();
            }
            data["features"].push_back(feature_id);
        }
    }

    StoreCreature(std::move(data), id);
    feature_hooks_.merge(hooks);
    ability_rules_.reset();
}


void TGameObjectFactory::ReadAction(nlohmann::json& json, TGameObjectId id)
{
    if (actions_.contains(id)) {
        throw std::invalid_argument("duplicate action definition");
    }
    auto action = std::make_shared<TAction>(action_reader_.ReadAction(json, *this));
    const auto definition = json.value("ability", "legacy_action." +
        std::string(TGameObjectIdManager::Instance().Name(id)));
    const auto source = json.value("source", std::string{});
    if (!source.empty() && source != "weapon") {
        throw std::invalid_argument("unsupported action source: " + source);
    }
    actions_.emplace(id, [action]([[maybe_unused]] const TGameObjectFactory& factory) { return action; });
    ability_bindings_.emplace(id, TAbilityBinding{definition, source == "weapon"});
    ability_rules_.reset();
}

std::shared_ptr<const TAbilityRules> TGameObjectFactory::AbilityRules() const
{
    if (ability_rules_) {
        return ability_rules_;
    }
    auto rules = std::make_shared<TAbilityRules>();
    rules->Time.RoundDurationSeconds = CreateCombatTiming().RoundSeconds();
    RegisterAbilitiesRules(*rules);
    RegisterFlankingRules(*rules);
    RegisterMovementRules(*rules);
    RegisterShieldActionsRules(*rules);
    RegisterIntimidationRules(*rules);
    RegisterTripRules(*rules);
    RegisterRemasterProfilesRules(*rules);
    rules->Lifecycle.Register(ELifecyclePhase::TurnStartExpiration,
        std::make_shared<const TLifecycleExecution>(TLifecycleExecution{
            .OwnerType = typeid(TCreature),
            .Run = RunAbilitiesExpireCreatureTurnStart,
        }));
    rules->Lifecycle.Register(ELifecyclePhase::TurnStart,
        std::make_shared<const TLifecycleExecution>(TLifecycleExecution{
            .OwnerType = typeid(TCreature),
            .Run = RunAbilitiesStartCreatureTurn,
        }));
    for (const auto& [id, hook] : feature_hooks_) {
        rules->Catalog.Register({id, EAbilityKind::Feature, {}});
        rules->FeatureHooks.emplace(id, hook);
    }
    std::vector<TGameObjectId> ids = AllKnown<TAction>();
    std::ranges::sort(ids, {}, [](TGameObjectId id) {
        return std::string(TGameObjectIdManager::Instance().Name(id));
    });
    for (auto id : ids) {
        const auto& binding = ability_bindings_.at(id);
        if (binding.Definition.starts_with("legacy_action.")) {
            rules->Catalog.Register({binding.Definition, EAbilityKind::Action, {}});
        }
        const auto& definition = rules->Catalog.Require(binding.Definition);
        if (definition.Kind != EAbilityKind::Action) {
            throw std::invalid_argument("action implementation kind mismatch: " + binding.Definition);
        }
        const auto name = std::string(TGameObjectIdManager::Instance().Name(id));
        rules->Catalog.Register({"legacy_feature." + name, EAbilityKind::Feature, {binding.Definition}});
        FLegacyAbilityRequirement requirement;
        if (binding.RequiresWeaponSource) {
            requirement = [](const TPlayer& actor, const TAbilitySourcePath& source) {
                const auto& creature = *actor.GetCreature();
                for (const auto& [field, weapons] : {
                        std::pair{"weapons", &creature.Weapons()},
                        std::pair{"natural_weapons", &creature.NaturalWeapons()}}) {
                    for (auto entry : *weapons) {
                        if (source.StartsWith(TAbilitySourcePath{}.Field(field).Entry(entry.Id().Value))) {
                            return true;
                        }
                    }
                }
                return false;
            };
        }
        rules->Executions.Register(binding.Definition, Create<TAction>(id), std::move(requirement));
    }
    ability_rules_ = std::move(rules);
    return ability_rules_;
}


void TGameObjectFactory::ReadBattleMap(nlohmann::json& json, TGameObjectId id)
{
    TBattleMap battle_map(json);
    battle_maps_.insert({id, [battle_map]([[maybe_unused]] const TGameObjectFactory& factory) {
        return battle_map;
    }});
}
