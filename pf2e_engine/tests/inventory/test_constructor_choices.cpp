#include <pf2e_engine/inventory/constructor_choices.h>
#include <pf2e_engine/common/construction.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

TFeatureSelection Feature(std::string_view name)
{
    return TFeatureSelection("constructor_choices." + std::string(name));
}

const TFeatureChoiceDescriptor& Field(const TConstructionReport& report, std::string_view name)
{
    const auto found = std::find_if(report.Fields.begin(), report.Fields.end(),
        [name](const auto& field) { return field.Name == name; });
    if (found == report.Fields.end()) {
        throw std::logic_error("missing constructor choice descriptor");
    }
    return *found;
}

bool HasIssue(const TConstructionReport& report, std::string_view parameter)
{
    return std::any_of(report.Issues.begin(), report.Issues.end(),
        [parameter](const auto& issue) {
            return issue.Parameter == parameter && !issue.Message.empty();
        });
}

std::vector<std::string> Features(const TConstructedCreature& creature)
{
    std::vector<std::string> result;
    for (const auto entry : creature.Features()) {
        result.push_back(entry->DefinitionId());
    }
    return result;
}

TEST(ConstructorChoicesTest, DefaultsDescribeValidateAndCreateTheSameSelection)
{
    const auto identity = EntityIdCounter().load(std::memory_order_relaxed);
    const auto report = DescribeConfiguredFighter();
    EXPECT_EQ(report, ValidateConfiguredFighter());
    EXPECT_TRUE(report.IsValid());
    EXPECT_EQ(report.Fields.size(), 3u);
    EXPECT_EQ(EntityIdCounter().load(std::memory_order_relaxed), identity);
    const auto& class_feat = Field(report, "ClassFeat");
    EXPECT_EQ(class_feat.State, EConstructionChoiceState::Enabled);
    EXPECT_EQ(class_feat.Selected, Feature("ConstructorPower"));
    EXPECT_TRUE(class_feat.UsesDefault);
    EXPECT_EQ(class_feat.Candidates, (std::vector<TFeatureSelection>{
        Feature("ConstructorPower"), Feature("ConstructorRush"),
    }));
    const auto& ambition = Field(report, "AmbitionFeat");
    EXPECT_EQ(ambition.Selected, Feature("ConstructorRush"));
    EXPECT_EQ(ambition.Dependencies, (std::vector<std::string>{"AncestryFeat"}));
    const auto fighter = CreateConfiguredFighter();
    EXPECT_EQ(fighter.Id().Value, identity);
    EXPECT_EQ(fighter.Name(), "Human fighter");
    EXPECT_EQ(fighter.Level(), 1);
    EXPECT_EQ(fighter.DefaultQuotient(), 10);
    ASSERT_EQ(fighter.Weapons().Size(), 1u);
    EXPECT_EQ((*fighter.Weapons().begin())->Name(), "Longsword");
    EXPECT_EQ(Features(fighter), (std::vector<std::string>{
        Feature("ConstructorPower").DefinitionId(),
        Feature("ConstructorAmbition").DefinitionId(),
        Feature("ConstructorRush").DefinitionId(),
    }));
}

TEST(ConstructorChoicesTest, ArbitraryEquipmentAndExplicitEmptyEquipmentStayIndependent)
{
    TConfiguredFighterOptions options;
    options.Equipment = std::vector<TWeapon>{
        TWeapon::Create({.Name = "Spear", .BaseDiceSize = 6, .Reach = 2}),
        TWeapon::Create({.Name = "Dagger", .BaseDiceSize = 4}),
    };
    EXPECT_TRUE(ValidateConfiguredFighter(options).IsValid());
    const auto first = CreateConfiguredFighter(options);
    const auto second = CreateConfiguredFighter(options);
    ASSERT_EQ(first.Weapons().Size(), 2u);
    EXPECT_EQ((*first.Weapons().begin())->Name(), "Spear");
    EXPECT_EQ((*first.Weapons().begin())->Reach(), 2);
    EXPECT_NE(first.Id(), second.Id());
    options.Equipment->front() = TWeapon::Create({.Name = "Changed input"});
    EXPECT_EQ((*first.Weapons().begin())->Name(), "Spear");
    EXPECT_EQ((*second.Weapons().begin())->Name(), "Spear");
    options.Equipment = std::vector<TWeapon>{};
    const auto unarmed = CreateConfiguredFighter(options);
    EXPECT_EQ(unarmed.Weapons().Size(), 0u);
    EXPECT_EQ(unarmed.Weapons().NextId().Value, 0u);
}

TEST(ConstructorChoicesTest, DisabledChoiceSkipsDefaultAndConsumesNoCollectionIdentity)
{
    TConfiguredFighterOptions options;
    options.AncestryFeat = Feature("ConstructorSkilled");
    const auto report = DescribeConfiguredFighter(options);
    ASSERT_TRUE(report.IsValid());
    const auto& disabled = Field(report, "AmbitionFeat");
    EXPECT_EQ(disabled.State, EConstructionChoiceState::Disabled);
    EXPECT_TRUE(disabled.Candidates.empty());
    EXPECT_FALSE(disabled.Selected.has_value());
    EXPECT_FALSE(disabled.UsesDefault);
    const auto fighter = CreateConfiguredFighter(options);
    EXPECT_EQ(fighter.Features().Size(), 2u);
    EXPECT_EQ(fighter.Features().NextId().Value, 2u);
    options.AmbitionFeat = Feature("ConstructorRush");
    EXPECT_TRUE(HasIssue(ValidateConfiguredFighter(options), "AmbitionFeat"));
    EXPECT_THROW(CreateConfiguredFighter(options), std::invalid_argument);
    options.AmbitionFeat = TFeatureSelection{};
    EXPECT_TRUE(HasIssue(ValidateConfiguredFighter(options), "AmbitionFeat"));
    EXPECT_THROW(CreateConfiguredFighter(options), std::invalid_argument);
}

TEST(ConstructorChoicesTest, WrongPoolUnknownActionAndEmptyRequiredSelectionsReject)
{
    for (const auto& selected : {
        Feature("ConstructorSkilled"), Feature("Missing"),
        Feature("ConstructorStrike"), TFeatureSelection{},
    }) {
        TConfiguredFighterOptions options;
        options.ClassFeat = selected;
        const auto report = ValidateConfiguredFighter(options);
        EXPECT_FALSE(report.IsValid());
        EXPECT_TRUE(HasIssue(report, "ClassFeat"));
        EXPECT_EQ(Field(report, "ClassFeat").State, EConstructionChoiceState::Enabled);
        EXPECT_FALSE(Field(report, "ClassFeat").Selected.has_value());
        EXPECT_THROW(CreateConfiguredFighter(options), std::invalid_argument);
    }
}

TEST(ConstructorChoicesTest, CrossChoiceRequirementsRejectDuplicatesWithoutAllocatingIdentity)
{
    const auto identity = EntityIdCounter().load(std::memory_order_relaxed);
    TConfiguredFighterOptions options;
    options.AmbitionFeat = Feature("ConstructorPower");
    const auto report = ValidateConfiguredFighter(options);
    EXPECT_FALSE(report.IsValid());
    EXPECT_FALSE(report.Issues.empty());
    EXPECT_THROW(CreateConfiguredFighter(options), std::invalid_argument);
    EXPECT_EQ(EntityIdCounter().load(std::memory_order_relaxed), identity);
    options.ClassFeat = Feature("ConstructorRush");
    EXPECT_TRUE(ValidateConfiguredFighter(options).IsValid());
    const auto fighter = CreateConfiguredFighter(options);
    EXPECT_EQ(fighter.Id().Value, identity);
    EXPECT_FALSE(Field(DescribeConfiguredFighter(options), "ClassFeat").UsesDefault);
}

TEST(ConstructorChoicesTest, MissingAndInvalidPredecessorsLeaveDependentChoicePending)
{
    const auto missing = DescribeRequiredConstructorChoice();
    EXPECT_FALSE(missing.IsValid());
    EXPECT_FALSE(missing.Complete);
    EXPECT_TRUE(HasIssue(missing, "AncestryFeat"));
    EXPECT_EQ(Field(missing, "BonusFeat").State, EConstructionChoiceState::Pending);
    EXPECT_FALSE(HasIssue(missing, "BonusFeat"));
    TRequiredConstructorChoiceOptions options;
    options.AncestryFeat = Feature("Missing");
    const auto invalid = ValidateRequiredConstructorChoice(options);
    EXPECT_EQ(Field(invalid, "BonusFeat").State, EConstructionChoiceState::Pending);
    EXPECT_FALSE(HasIssue(invalid, "BonusFeat"));
    options.AncestryFeat = Feature("ConstructorSkilled");
    EXPECT_TRUE(ValidateRequiredConstructorChoice(options).IsValid());
    const auto skilled = CreateRequiredConstructorChoice(options);
    EXPECT_EQ(skilled.Features().Size(), 1u);
    options.AncestryFeat = Feature("ConstructorAmbition");
    EXPECT_TRUE(HasIssue(ValidateRequiredConstructorChoice(options), "BonusFeat"));
}

TEST(ConstructorChoicesTest, DisabledChoiceNeverEvaluatesItsFailingPool)
{
    const auto report = DescribeConditionalConstructorChoice();
    EXPECT_TRUE(report.IsValid());
    EXPECT_EQ(Field(report, "BonusFeat").State, EConstructionChoiceState::Disabled);
    const auto creature = CreateConditionalConstructorChoice();
    EXPECT_EQ(creature.Features().Size(), 0u);
    TConditionalConstructorChoiceOptions options;
    options.Enable = true;
    EXPECT_TRUE(HasIssue(ValidateConditionalConstructorChoice(options), "BonusFeat"));
    EXPECT_THROW(CreateConditionalConstructorChoice(options), std::invalid_argument);
}

TEST(ConstructorChoicesTest, ParameterDependentPoolsExposeOnlyLegalCandidates)
{
    TLeveledConstructorChoiceOptions options;
    options.ClassFeat = Feature("ConstructorRush");
    auto report = DescribeLeveledConstructorChoice(options);
    EXPECT_FALSE(report.IsValid());
    EXPECT_EQ(Field(report, "ClassFeat").Candidates,
              (std::vector<TFeatureSelection>{Feature("ConstructorPower")}));
    options.CharacterLevel = 2;
    report = DescribeLeveledConstructorChoice(options);
    EXPECT_TRUE(report.IsValid());
    EXPECT_EQ(Field(report, "ClassFeat").Candidates.size(), 2u);
    EXPECT_EQ(CreateLeveledConstructorChoice(options).Level(), 2);
}

TEST(ConstructorChoicesTest, ExplicitInitializersAndClassDefaultsFailDuringValidation)
{
    const auto identity = EntityIdCounter().load(std::memory_order_relaxed);
    TCheckedConstructorFieldsOptions fields;
    fields.Divisor = 0;
    const auto explicit_failure = DescribeCheckedConstructorFields(fields);
    EXPECT_EQ(explicit_failure, ValidateCheckedConstructorFields(fields));
    EXPECT_FALSE(explicit_failure.IsValid());
    EXPECT_FALSE(explicit_failure.Issues.empty());
    EXPECT_THROW(CreateCheckedConstructorFields(fields), std::invalid_argument);
    TCheckedConstructorDefaultsOptions defaults;
    defaults.CharacterLevel = 0;
    const auto default_failure = DescribeCheckedConstructorDefaults(defaults);
    EXPECT_EQ(default_failure, ValidateCheckedConstructorDefaults(defaults));
    EXPECT_FALSE(default_failure.IsValid());
    EXPECT_FALSE(default_failure.Issues.empty());
    EXPECT_THROW(CreateCheckedConstructorDefaults(defaults), std::invalid_argument);
    EXPECT_THROW(TConstructedCreature::Create({.Level = 0}), std::domain_error);
    EXPECT_EQ(EntityIdCounter().load(std::memory_order_relaxed), identity);
    EXPECT_EQ(CreateCheckedConstructorFields().BodyQuotient(), 10);
    EXPECT_EQ(CreateCheckedConstructorDefaults().DefaultQuotient(), 10);
}

TEST(ConstructorChoicesTest, FeatureMembershipTestsDirectSelectionsAndClassParametersRemainValues)
{
    const auto grouped = CreateConstructorMembership();
    EXPECT_FALSE(grouped.HasDirectTraining());
    EXPECT_EQ(grouped.Profile().Value(), 5);
    TAbilityCatalog catalog;
    RegisterConstructorChoicesAbilities(catalog);
    const auto grants = CollectAbilities(grouped, catalog).Grants();
    EXPECT_TRUE(std::any_of(grants.begin(), grants.end(), [](const auto& grant) {
        return grant.DefinitionId == Feature("ConstructorPowerAttack").DefinitionId();
    }));
    TConstructorMembershipOptions options;
    options.Selected = std::vector<TFeatureSelection>{Feature("ConstructorPower")};
    options.SelectedProfile = TConstructorProfile::Create({.Divisor = 5});
    const auto direct = CreateConstructorMembership(options);
    EXPECT_TRUE(direct.HasDirectTraining());
    EXPECT_EQ(direct.Profile().Value(), 2);
}

TEST(ConstructorChoicesTest, UnparameterizedFactoriesKeepIndependentEntityAndWeaponValues)
{
    const auto first = CreateConstructedWolf();
    const auto second = CreateConstructedWolf();
    EXPECT_NE(first.Id(), second.Id());
    EXPECT_EQ(first.Name(), "Wolf");
    EXPECT_EQ(second.Name(), "Wolf");
    ASSERT_EQ(first.Weapons().Size(), 1u);
    ASSERT_EQ(second.Weapons().Size(), 1u);
    EXPECT_NE(&*(*first.Weapons().begin()), &*(*second.Weapons().begin()));
    EXPECT_EQ((*second.Weapons().begin())->Name(), "Jaws");
}

TEST(ConstructorChoicesTest, RestorePreservesExplicitEmptySelectionsAndSparseCollectionIds)
{
    TIdCollection<TFeatureSelection> features;
    features.RestoreEntry({4}, TFeatureSelection{});
    features.RestoreEntry({9}, Feature("ConstructorPower"));
    features.RestoreNextId({20});
    const auto creature = TConstructedCreature::Create({.Features = features});
    const auto json = creature.ToJson();
    TGameObjectFactory factory;
    const auto restored = TConstructedCreature::Restore(json, factory);
    EXPECT_EQ(restored.ToJson(), json);
    EXPECT_EQ(restored.Features().Size(), 2u);
    EXPECT_EQ(restored.Features().NextId().Value, 20u);
    ASSERT_NE(restored.Features().Find({4}), nullptr);
    EXPECT_EQ(restored.Features().Find({4})->DefinitionId(), "");
    EXPECT_EQ(restored.Id(), creature.Id());
}

TEST(ConstructorChoicesTest, NestedInitializersOverrideInvalidNestedDefaultsBeforeEvaluation)
{
    const auto identity = EntityIdCounter().load(std::memory_order_relaxed);
    const auto report = DescribeNestedConstructorDefaults();
    EXPECT_TRUE(report.IsValid());
    EXPECT_EQ(report, ValidateNestedConstructorDefaults());
    EXPECT_EQ(EntityIdCounter().load(std::memory_order_relaxed), identity);
    const auto creature = CreateNestedConstructorDefaults();
    EXPECT_EQ(creature.Id().Value, identity);
    EXPECT_EQ(creature.Profile().Value(), 5);
    TNestedConstructorDefaultsOptions options;
    options.Divisor = 5;
    const auto selected = CreateNestedConstructorDefaults(options);
    EXPECT_EQ(selected.Profile().Value(), 2);
    options.Divisor = 0;
    EXPECT_FALSE(ValidateNestedConstructorDefaults(options).IsValid());
    EXPECT_THROW(CreateNestedConstructorDefaults(options), std::invalid_argument);
    const auto overridden = TNestedConstructedCreature::Create({
        .Divisor = 0,
        .Profile = TConstructorRequiredProfile::Create({.Divisor = 10}),
    });
    EXPECT_EQ(overridden.Profile().Value(), 1);
    TGameObjectFactory factory;
    const auto restored = TNestedConstructedCreature::Restore(overridden.ToJson(), factory);
    EXPECT_EQ(restored.ToJson(), overridden.ToJson());
}

}
