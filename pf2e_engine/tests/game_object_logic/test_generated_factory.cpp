#include <gtest/gtest.h>

#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/feat.h>
#include <pf2e_engine/transformation/transformator.h>
#include <cpp_config.h>

#include "../test_lib/mock_interaction_system.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace generated_factory_fixture {

static_assert(!std::is_copy_constructible_v<TCreature>);
static_assert(std::is_move_constructible_v<TCreature>);

TGameObjectId NameId(const std::string& name) {
    return TGameObjectIdManager::Instance().Register(name);
}

struct TValue {
    int amount;

    static TValue FromJson(const nlohmann::json& json, [[maybe_unused]] const TGameObjectFactory& factory) {
        return {json.at("amount").get<int>()};
    }
};

struct TOtherValue {
    int amount;

    static TOtherValue FromJson(const nlohmann::json& json, [[maybe_unused]] const TGameObjectFactory& factory) {
        return {json.at("amount").get<int>()};
    }
};

struct TBundle {
    TValue value;

    static TBundle FromJson(const nlohmann::json& json, const TGameObjectFactory& factory) {
        const auto& value = json.at("value");
        return {value.is_string()
            ? factory.Create<TValue>(NameId(value.get<std::string>()))
            : TValue::FromJson(value, factory)};
    }
};

struct TLinkedValue {
    int amount;

    static TLinkedValue FromJson(const nlohmann::json& json, const TGameObjectFactory& factory) {
        if (json.contains("next")) {
            return factory.Create<TLinkedValue>(NameId(json.at("next").get<std::string>()));
        }
        return {json.at("amount").get<int>()};
    }
};

struct TDelegatedValue {
    int amount;
    inline static const TGameObjectFactory* delegate = nullptr;

    static TDelegatedValue FromJson(const nlohmann::json& json, const TGameObjectFactory& factory) {
        if (delegate != nullptr && delegate != &factory) {
            return delegate->Create<TDelegatedValue>(NameId("delegated"));
        }
        return {json.at("amount").get<int>()};
    }
};

class TGeneratedFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        directory_ = std::filesystem::temp_directory_path() /
            ("ttrpg-generated-factory-" + std::to_string(stamp));
        ASSERT_TRUE(std::filesystem::create_directory(directory_));
    }

    void TearDown() override {
        TDelegatedValue::delegate = nullptr;
        std::filesystem::remove_all(directory_);
    }

    std::filesystem::path Write(const std::string& filename, const nlohmann::json& json) const {
        const auto path = directory_ / filename;
        std::ofstream output(path);
        output.exceptions(std::ios::failbit | std::ios::badbit);
        output << json;
        output.close();
        return path;
    }

    void AddBundleDefinitions(TGameObjectFactory& factory) const {
        factory.RegisterType<TValue>("fixture_value");
        factory.RegisterType<TBundle>("fixture_bundle");
        factory.AddSource(Write("bundle.json", {
            {"type", "fixture_bundle"}, {"name", "bundle"},
            {"fixture_bundle", {{"value", "value"}}},
        }));
        factory.AddSource(Write("value.json", {
            {"type", "fixture_value"}, {"name", "value"},
            {"fixture_value", {{"amount", 17}}},
        }));
    }

    void AddCreatureAssets(TGameObjectFactory& factory) const {
        const std::filesystem::path data{kRootDirPath + "/pf2e_engine/data"};
        for (const auto* source : {"inventory/material/leather.json", "inventory/armor/unarmored.json",
                "inventory/weapon/longsword.json", "creatures/races/human.json", "creatures/classes/fighter.json"}) {
            factory.AddSource(data / source);
        }
    }

    nlohmann::json CreatureDefinition(const std::string& name) const {
        std::ifstream input(kRootDirPath + "/pf2e_engine/data/creatures/warrior.json");
        auto definition = nlohmann::json::parse(input);
        definition["name"] = name;
        definition["pf2e_creature"]["creature_data"]["features"] = nlohmann::json::array();
        return definition;
    }

    std::filesystem::path directory_;
};

TEST_F(TGeneratedFactoryTest, InlineClassNeedsNoRegistration) {
    TGameObjectFactory factory;
    const auto bundle = TBundle::FromJson({{"value", {{"amount", 11}}}}, factory);
    EXPECT_EQ(bundle.value.amount, 11);
    EXPECT_TRUE(factory.AllKnown<TValue>().empty());
}

TEST_F(TGeneratedFactoryTest, ReferencesResolveAfterAllDefinitionsLoad) {
    TGameObjectFactory factory;
    AddBundleDefinitions(factory);
    const auto bundle = factory.Create<TBundle>(NameId("bundle"));
    EXPECT_EQ(bundle.value.amount, 17);
    ASSERT_EQ(factory.AllKnown<TValue>().size(), 1u);
    EXPECT_EQ(factory.AllKnown<TValue>().front(), NameId("value"));
}

TEST_F(TGeneratedFactoryTest, InstancesDoNotShareMutableState) {
    TGameObjectFactory factory;
    AddBundleDefinitions(factory);
    auto first = factory.Create<TBundle>(NameId("bundle"));
    first.value.amount = 3;
    EXPECT_EQ(first.value.amount, 3);
    EXPECT_EQ(factory.Create<TBundle>(NameId("bundle")).value.amount, 17);
}

TEST_F(TGeneratedFactoryTest, MovingAndCopyingUsesTheDestinationFactory) {
    auto original = std::make_unique<TGameObjectFactory>();
    AddBundleDefinitions(*original);
    TGameObjectFactory copy = *original;
    TGameObjectFactory moved = std::move(*original);
    original.reset();
    EXPECT_EQ(copy.Create<TBundle>(NameId("bundle")).value.amount, 17);
    EXPECT_EQ(moved.Create<TBundle>(NameId("bundle")).value.amount, 17);
}

TEST_F(TGeneratedFactoryTest, ReferencesAreScopedByCppType) {
    TGameObjectFactory factory;
    factory.RegisterType<TValue>("fixture_value");
    factory.RegisterType<TOtherValue>("fixture_other");
    factory.RegisterType<TBundle>("fixture_bundle");
    factory.AddSource(Write("value.json", {
        {"type", "fixture_value"}, {"name", "shared_name"},
        {"fixture_value", {{"amount", 7}}},
    }));
    factory.AddSource(Write("other.json", {
        {"type", "fixture_other"}, {"name", "shared_name"},
        {"fixture_other", {{"amount", 9}}},
    }));
    factory.AddSource(Write("bundle.json", {
        {"type", "fixture_bundle"}, {"name", "shared_name"},
        {"fixture_bundle", {{"value", "shared_name"}}},
    }));
    EXPECT_EQ(factory.Create<TValue>(NameId("shared_name")).amount, 7);
    EXPECT_EQ(factory.Create<TOtherValue>(NameId("shared_name")).amount, 9);
    EXPECT_EQ(factory.Create<TBundle>(NameId("shared_name")).value.amount, 7);
}

TEST_F(TGeneratedFactoryTest, RegistrationIsIdempotentAndRejectsConflicts) {
    TGameObjectFactory factory;
    factory.RegisterType<TValue>("fixture_value");
    EXPECT_NO_THROW(factory.RegisterType<TValue>("fixture_value"));
    EXPECT_THROW(factory.RegisterType<TOtherValue>("fixture_value"), std::invalid_argument);
    EXPECT_THROW(factory.RegisterType<TValue>("different_name"), std::invalid_argument);
    EXPECT_THROW(factory.RegisterType<TOtherValue>("pf2e_armor"), std::invalid_argument);
    EXPECT_THROW(factory.RegisterType<TArmor>("custom_armor"), std::invalid_argument);
}

TEST_F(TGeneratedFactoryTest, MissingRegistrationAndMissingReferencesAreDiagnosed) {
    TGameObjectFactory factory;
    EXPECT_THROW(factory.Create<TValue>(NameId("absent")), std::runtime_error);
    factory.RegisterType<TValue>("fixture_value");
    EXPECT_THROW(factory.Create<TValue>(NameId("absent")), std::runtime_error);
}

TEST_F(TGeneratedFactoryTest, DuplicateDefinitionsAreRejected) {
    TGameObjectFactory factory;
    AddBundleDefinitions(factory);
    EXPECT_THROW(factory.AddSource(directory_ / "value.json"), std::invalid_argument);
    EXPECT_EQ(factory.Create<TValue>(NameId("value")).amount, 17);
}

TEST_F(TGeneratedFactoryTest, InvalidEnvelopeAndPayloadAreRejected) {
    TGameObjectFactory factory;
    factory.RegisterType<TValue>("fixture_value");
    EXPECT_THROW(factory.AddSource(Write("invalid_name.json", {
        {"type", "fixture_value"}, {"name", 42}, {"fixture_value", {{"amount", 1}}},
    })), std::invalid_argument);
    EXPECT_THROW(factory.AddSource(Write("invalid_payload.json", {
        {"type", "fixture_value"}, {"name", "invalid"}, {"fixture_value", 1},
    })), std::invalid_argument);
    factory.AddSource(Write("invalid_field.json", {
        {"type", "fixture_value"}, {"name", "invalid_field"},
        {"fixture_value", {{"amount", "not_an_integer"}}},
    }));
    EXPECT_THROW(factory.Create<TValue>(NameId("invalid_field")), nlohmann::json::type_error);
}

TEST_F(TGeneratedFactoryTest, SelfReferencesAreRejectedWithCycleDiagnostic) {
    TGameObjectFactory factory;
    factory.RegisterType<TLinkedValue>("linked_value");
    factory.AddSource(Write("self.json", {
        {"type", "linked_value"}, {"name", "self_reference"},
        {"linked_value", {{"next", "self_reference"}}},
    }));
    try {
        factory.Create<TLinkedValue>(NameId("self_reference"));
        FAIL() << "Expected recursive definition to fail";
    } catch (const std::runtime_error& error) {
        EXPECT_EQ(std::string(error.what()),
            "cyclic generated object reference: self_reference -> self_reference");
    }
}

TEST_F(TGeneratedFactoryTest, MutualCyclesUnwindConstructionState) {
    TGameObjectFactory factory;
    factory.RegisterType<TLinkedValue>("linked_value");
    factory.AddSource(Write("first.json", {
        {"type", "linked_value"}, {"name", "first"},
        {"linked_value", {{"next", "second"}}},
    }));
    factory.AddSource(Write("second.json", {
        {"type", "linked_value"}, {"name", "second"},
        {"linked_value", {{"next", "first"}}},
    }));
    for (int attempt = 0; attempt < 2; ++attempt) {
        try {
            factory.Create<TLinkedValue>(NameId("first"));
            FAIL() << "Expected recursive definitions to fail";
        } catch (const std::runtime_error& error) {
            EXPECT_EQ(std::string(error.what()),
                "cyclic generated object reference: first -> second -> first");
        }
    }
}

TEST_F(TGeneratedFactoryTest, FailedNestedLookupDoesNotPoisonLaterConstruction) {
    TGameObjectFactory factory;
    factory.RegisterType<TLinkedValue>("linked_value");
    factory.AddSource(Write("first.json", {
        {"type", "linked_value"}, {"name", "first"},
        {"linked_value", {{"next", "later"}}},
    }));
    EXPECT_THROW(factory.Create<TLinkedValue>(NameId("first")), std::runtime_error);
    factory.AddSource(Write("later.json", {
        {"type", "linked_value"}, {"name", "later"},
        {"linked_value", {{"amount", 23}}},
    }));
    EXPECT_EQ(factory.Create<TLinkedValue>(NameId("first")).amount, 23);
}

TEST_F(TGeneratedFactoryTest, SameTypeAndIdInAnotherFactoryIsNotACycle) {
    TGameObjectFactory first;
    TGameObjectFactory second;
    first.RegisterType<TDelegatedValue>("delegated_value");
    second.RegisterType<TDelegatedValue>("delegated_value");
    const auto definition = Write("delegated.json", {
        {"type", "delegated_value"}, {"name", "delegated"},
        {"delegated_value", {{"amount", 31}}},
    });
    first.AddSource(definition);
    second.AddSource(definition);
    TDelegatedValue::delegate = &second;
    EXPECT_EQ(first.Create<TDelegatedValue>(NameId("delegated")).amount, 31);
}

TEST_F(TGeneratedFactoryTest, ImportedCreatureDefinitionsCreateIndependentEntities) {
    TGameObjectFactory factory;
    AddCreatureAssets(factory);
    factory.AddSource(Write("creature.json", CreatureDefinition("generated_creature")));
    TCreature first = factory.Create<TCreature>(NameId("generated_creature"));
    const TCreature second = factory.Create<TCreature>(NameId("generated_creature"));
    EXPECT_NE(first.Id().Value, 0u);
    EXPECT_NE(first.Id(), second.Id());
    TMockInteractionSystem io;
    TTransformator transformator(io);
    auto ctx = transformator.ExecutionContext();
    ctx.Invoke(first.Hitpoints(), &THitPoints::DealDamage, 5);
    EXPECT_EQ(std::as_const(first).Hitpoints().Current(), second.Hitpoints().Current() - 5);
    EXPECT_EQ(factory.Create<TCreature>(NameId("generated_creature")).Hitpoints()->Current(),
              second.Hitpoints().Current());
}

TEST_F(TGeneratedFactoryTest, CreatureSnapshotsRestoreIdentityAndStateWithoutReinitializing) {
    TGameObjectFactory factory;
    AddCreatureAssets(factory);
    factory.AddSource(Write("creature.json", CreatureDefinition("snapshot_creature")));
    TCreature original = factory.Create<TCreature>(NameId("snapshot_creature"));
    TMockInteractionSystem io;
    TTransformator transformator(io);
    auto ctx = transformator.ExecutionContext();
    ctx.Invoke(original.Hitpoints(), &THitPoints::DealDamage, 6);
    const auto snapshot = original.ToJson();
    const TCreature restored = TCreature::Restore(snapshot, factory);
    EXPECT_EQ(restored.Id(), original.Id());
    EXPECT_EQ(restored.ToJson(), snapshot);
    EXPECT_EQ(restored.Hitpoints().Current(), 15);
    EXPECT_NE(factory.Create<TCreature>(NameId("snapshot_creature")).Id(), restored.Id());
}

TEST_F(TGeneratedFactoryTest, LegacyCreatureDataEnvelopeImportsAsGeneratedCreature) {
    TGameObjectFactory factory;
    AddCreatureAssets(factory);
    const auto definition = CreatureDefinition("unused");
    factory.AddSource(Write("creature_data.json", {
        {"type", "pf2e_creature_data"}, {"name", "legacy_profile"},
        {"pf2e_creature_data", definition.at("pf2e_creature").at("creature_data")}
    }));
    const TCreature creature = factory.Create<TCreature>(NameId("legacy_profile"));
    EXPECT_EQ(creature.Level(), 1);
    EXPECT_EQ(creature.Hitpoints().Current(), 21);
    EXPECT_EQ(factory.AllKnown<TCreature>(), std::vector<TGameObjectId>{NameId("legacy_profile")});
    EXPECT_THROW(factory.AddSource(directory_ / "creature_data.json"), std::invalid_argument);
}

TEST_F(TGeneratedFactoryTest, CreatureImportsUseTheCopiedOrMovedFactoryForNestedReferences) {
    auto original = std::make_unique<TGameObjectFactory>();
    AddCreatureAssets(*original);
    original->AddSource(Write("armor.json", {
        {"type", "pf2e_armor"}, {"name", "deferred_armor"},
        {"pf2e_armor", {{"category", "Unarmored"}, {"material", "destination_material"},
                        {"armor_class_bonus", 0}, {"dexterity_cap", 10}}}
    }));
    auto definition = CreatureDefinition("copied_creature");
    definition["pf2e_creature"]["creature_data"]["armor"] = "deferred_armor";
    original->AddSource(Write("creature.json", definition));
    TGameObjectFactory copy = *original;
    TGameObjectFactory moved = std::move(*original);
    copy.AddSource(Write("copy_material.json", {
        {"type", "pf2e_material"}, {"name", "destination_material"}, {"pf2e_material", {{"hardness", 3}}}
    }));
    moved.AddSource(Write("moved_material.json", {
        {"type", "pf2e_material"}, {"name", "destination_material"}, {"pf2e_material", {{"hardness", 9}}}
    }));
    original.reset();
    const TCreature first = copy.Create<TCreature>(NameId("copied_creature"));
    const TCreature second = moved.Create<TCreature>(NameId("copied_creature"));
    EXPECT_EQ(first.Armor().Material().Hardness(), 3);
    EXPECT_EQ(second.Armor().Material().Hardness(), 9);
    EXPECT_NE(first.Id(), second.Id());
}

TEST_F(TGeneratedFactoryTest, LegacyFeatsBecomeSelectionsAndImmutableCatalogHooks) {
    TGameObjectFactory factory;
    AddCreatureAssets(factory);
    auto definition = CreatureDefinition("legacy_hook_creature");
    definition["pf2e_creature"]["feats"] = nlohmann::json::array({{
        {"name", "Imported feature"}, {"block", "weapon_damage_roll"},
        {"pipeline", nlohmann::json::array({{{"name", "done"}, {"type", "terminate"}}})}
    }});
    factory.AddSource(Write("creature.json", definition));
    const TCreature creature = factory.Create<TCreature>(NameId("legacy_hook_creature"));
    const auto rules = factory.AbilityRules();
    const auto selected = CollectAbilities(creature, rules->Catalog);
    ASSERT_EQ(selected.Features().size(), 1u);
    const std::string feature = "legacy_feat.legacy_hook_creature.0";
    EXPECT_EQ(selected.Features().front().DefinitionId, feature);
    EXPECT_EQ(rules->FeatureHooks.at(feature)->name, "Imported feature");
    EXPECT_TRUE(selected.Grants().empty());

    TGameObjectFactory copy = factory;
    definition["name"] = "second_hook_creature";
    copy.AddSource(Write("second_creature.json", definition));
    EXPECT_EQ(copy.AbilityRules()->FeatureHooks.size(), 2u);
    EXPECT_EQ(factory.AbilityRules()->FeatureHooks.size(), 1u);
    EXPECT_EQ(rules->FeatureHooks.size(), 1u);
}

TEST_F(TGeneratedFactoryTest, CreatureCreationRejectsMissingActionImplementations) {
    TGameObjectFactory factory;
    AddCreatureAssets(factory);
    auto definition = CreatureDefinition("missing_execution");
    definition["pf2e_creature"]["creature_data"]["features"] = {"abilities.WolfActions"};
    factory.AddSource(Write("missing.json", definition));
    EXPECT_THROW(factory.Create<TCreature>(NameId("missing_execution")), std::invalid_argument);
    factory.AddSource(Write("valid.json", CreatureDefinition("after_failure")));
    EXPECT_EQ(factory.Create<TCreature>(NameId("after_failure")).Hitpoints()->Current(), 21);
}

}
