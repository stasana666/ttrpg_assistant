#include <gtest/gtest.h>

#include "compile_helpers.h"

#include <ttrpg/abilities.h>
#include <ttrpg/parser.h>
#include <ttrpg/program.h>

#include <stdexcept>
#include <string>

namespace {

TLoadedSchemas Rules(const std::string& source, const std::string& stem = "rules") {
    return LoadRules(stem, source);
}

std::string Header(const TLoadedSchemas& loaded) {
    return EmitHeaderText(loaded, "rules");
}

std::string Implementation(const TLoadedSchemas& loaded) {
    return EmitImplText(loaded, "rules");
}

void Rejects(const std::string& source, const std::string& diagnostic) {
    ExpectRejected(source, diagnostic, [&] { Header(Rules(source)); });
}

}

TEST(AbilityCompilerTest, ParsesReusableDefinitionsAndGrantSources) {
    auto loaded = Rules(R"(
        action Strike;
        reaction Opportunity;
        feature BasicActions { grant Strike; }
        feature Training { grant Opportunity; grant BasicActions; }
        GameEntity TActor {
            collection<Feature> Features;
            grant BasicActions;
            grant @self.Features;
        }
    )");
    ASSERT_EQ(loaded.Primary.Abilities.size(), 4u);
    EXPECT_EQ(loaded.Primary.Abilities[0].Kind, EAbilityDeclarationKind::Action);
    EXPECT_EQ(loaded.Primary.Abilities[1].Kind, EAbilityDeclarationKind::Reaction);
    EXPECT_EQ(loaded.Primary.Abilities[2].Kind, EAbilityDeclarationKind::Feature);
    ASSERT_EQ(loaded.Primary.Abilities[3].Grants.size(), 2u);
    EXPECT_NO_THROW(ValidateAbilityDefinitions(loaded));
    const auto compiled = CompileClass(loaded.Primary.Classes[0], loaded);
    ASSERT_EQ(compiled.Grants.size(), 2u);
    EXPECT_EQ(compiled.Grants[0].Kind, TCompiledGrant::EKind::Definition);
    EXPECT_EQ(compiled.Grants[0].DefinitionId, "rules.BasicActions");
    EXPECT_EQ(compiled.Grants[1].Kind, TCompiledGrant::EKind::Collection);
    EXPECT_EQ(compiled.Grants[1].Fields, std::vector<std::string>{"features"});
    EXPECT_EQ(compiled.Fields[0].CppType, "TIdCollection<TFeatureSelection>");
}

TEST(AbilityCompilerTest, GeneratesLiveProviderTraversalWithStableCollectionIds) {
    auto loaded = Rules(R"(
        action Strike;
        class TWeapon { grant Strike; }
        class TSelection { Feature ChosenFeature; }
        class TActor {
            collection<TWeapon> NaturalWeapons;
            TSelection Selection;
            grant @self.NaturalWeapons;
            grant @self.Selection.ChosenFeature;
        }
    )");
    const auto compiled = CompileClass(loaded.Primary.Classes[2], loaded);
    ASSERT_EQ(compiled.Grants.size(), 2u);
    EXPECT_EQ(compiled.Grants[1].Accessor, "(*this).Selection().ChosenFeature()");
    EXPECT_EQ(compiled.Grants[1].Fields, (std::vector<std::string>{"selection", "chosen_feature"}));
}

TEST(AbilityCompilerTest, BuildsFeatureSelectionsAndCollectionsInNamedInstances) {
    auto loaded = Rules(R"(
        action Strike;
        feature Training { grant Strike; }
        class TWeapon { string Name; grant Strike; }
        GameEntity TActor {
            Feature Primary = Training;
            collection<Feature> Features = [Training];
            collection<TWeapon> Weapons;
            grant @self.Primary;
            grant @self.Features;
            grant @self.Weapons;
        }
        TActor Fighter {
            Features = [Training,];
            Weapons = [TWeapon { Name = "Sword"; }];
        }
        TActor Wolf { Features = []; }
    )");
    const auto compiled = CompileClass(loaded.Primary.Classes[1], loaded);
    EXPECT_EQ(compiled.Fields[0].Initializer, "TFeatureSelection(\"rules.Training\")");
}

TEST(AbilityCompilerTest, RejectsInvalidDefinitionGraphs) {
    Rejects("feature A { grant B; } feature B { grant A; }", "cyclic feature grants");
    Rejects("feature A { grant A; }", "cyclic feature grants");
    Rejects("feature A { grant Missing; }", "unknown ability definition");
    Rejects("action A; reaction A;", "duplicate ability definition");
    Rejects("action A; feature B { grant A; grant A; }", "duplicate feature grant");
    Rejects("feature A { grant @self.Features; }", "feature grants must name");
    Rejects("class A {} action A;", "conflicts with existing symbol (type)");
    Rejects("class Feature {}", "reserved");
    Rejects("feature Feature {}", "reserved builtin");
    Rejects("action A {}", "expected ';'");
}

TEST(AbilityCompilerTest, RejectsUnstableOrUnsupportedGrantSources) {
    Rejects("class TActor { grant Missing; }", "unknown ability definition");
    Rejects("action A; class TActor { grant A; grant A; }", "duplicate grant source");
    Rejects("class TActor { Feature F; grant @self.F; grant @self.F; }", "duplicate grant source");
    Rejects("class TActor { int Number; grant @self.Number; }", "must contain Feature");
    Rejects("class TActor { Feature F; grant @other.F; }", "rooted at @self");
    Rejects("class TActor { Feature F; grant @self.Unknown; }", "unknown grant field");
    Rejects("class TActor { grant @self; }", "select a stored field");
    Rejects("class TActor { Feature F; grant @self.F(); }", "stored member path");
    Rejects("class TActor { collection<Feature> F; grant @self.F[0]; }", "stored member path");
    Rejects("feature A {} class TActor { derive Feature F = A; grant @self.F; }", "derived values");
    Rejects("class TActor { Feature F; grant @self.F.Nested; }", "traverse stored class fields");
    Rejects("class TEmpty {} class TActor { TEmpty Empty; grant @self.Empty; }", "must contain Feature");
}

TEST(AbilityCompilerTest, RejectsInvalidFeatureValuesAndCollections) {
    Rejects("action A; class TActor { Feature F = A; }", "feature definition");
    Rejects("reaction A; class TActor { collection<Feature> F = [A]; }", "feature definition");
    Rejects("class TActor { Feature F = Missing; }", "unknown ability definition");
    Rejects("class TActor { Feature F = 42; }", "expected 'Feature'");
    Rejects("class TActor { collection<Feature> F = [42]; }", "expected 'Feature'");
    Rejects("class TActor { collection<Feature> F = 42; }", "list literal");
    Rejects("class TActor { Feature F = []; }", "collection field initializer");
    Rejects("class TActor { set<Feature> F; }", "set elements must be");
    Rejects("enum E { A } class TActor { map<E, Feature> F; }", "Feature");
    Rejects("variant TChoice { A { Feature F; } }", "Feature");
    Rejects("feature A {} class TActor { collection<Feature> F; } TActor Example { F = [Missing]; }", "unknown ability definition");
}

TEST(AbilityCompilerTest, RejectsAmbiguousRegistrationNames) {
    auto loaded = Rules("");
    loaded.ByStem["a-b"] = {};
    loaded.ByStem["a_b"] = {};
    EXPECT_THROW(Header(loaded), std::runtime_error);
    Rejects("class RegisterRulesAbilities {}", "conflicts with existing symbol");
}

TEST(AbilityCompilerTest, EscapesDefinitionIdsAndProvenance) {
    const auto loaded = Rules("action Strike; feature Training { grant Strike; } class TActor { grant Training; }", "odd\"path\\name\n");
    const auto implementation = Implementation(loaded);
    EXPECT_NE(implementation.find("\"odd\\\"path\\\\name\\n.Strike\""), std::string::npos);
    EXPECT_NE(Header(loaded).find("\"TActor.grant.odd\\\"path\\\\name\\n.Training\""), std::string::npos);
}
