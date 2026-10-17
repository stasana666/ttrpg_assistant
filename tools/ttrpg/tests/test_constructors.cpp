#include <gtest/gtest.h>

#include "compile_helpers.h"

#include <ttrpg/constructors.h>
#include <ttrpg/program.h>

#include <stdexcept>
#include <string>

namespace {

const std::string kTypes = R"(
    feature First;
    feature Second;
    feature Optional;
    enum EKind { Simple, Complex }
    class TItem { string Name; int Value = 3; }
    class TPool {
        collection<Feature> Values;
        query bool Contains(Feature Value) { return @self.Values.has(Value); }
        query TPool Positive(int Value) { require Value > 0; return @self; }
    }
    TItem Blade { Name = "blade"; }
    TPool Pool { Values = [First, Second]; }
    GameEntity TActor {
        int Level = 1;
        int Ratio = 10 / @self.Level;
        bool Included;
        collection<TItem> Items;
        collection<Feature> Features;
    }
)";

TLoadedSchemas Rules(const std::string& source) {
    return LoadRules("constructors", kTypes + source);
}

std::string Header(const TLoadedSchemas& loaded) {
    return EmitHeaderText(loaded, "constructors");
}

std::string Implementation(const TLoadedSchemas& loaded) {
    return EmitImplText(loaded, "constructors");
}

const TInstanceDecl& Last(const TLoadedSchemas& loaded) {
    return loaded.Primary.Instances.back();
}

}

TEST(ConstructorTest, ParsesParametersChoicesAndRequirementPrefix) {
    const auto loaded = Rules(R"(
        TActor Fighter(
            collection<TItem> Equipment = [Blade, TItem { Name = "other"; }],
            choice Feature Training from Pool.Values = First,
            choice Feature Extra from [First, Second] when Training != Second = Second,
        ) {
            requires Training != Extra;
            Items = Equipment;
            Features = [Training, Extra];
        }
    )");
    const auto& value = Last(loaded);
    EXPECT_TRUE(value.Parameterized);
    ASSERT_EQ(value.Parameters.size(), 3u);
    EXPECT_EQ(value.Parameters[0].Field.Container, EContainer::Collection);
    EXPECT_EQ(value.Parameters[0].Field.Init->Args.size(), 2u);
    EXPECT_TRUE(value.Parameters[2].Choice);
    EXPECT_TRUE(value.Parameters[2].When);
    ASSERT_EQ(value.Requirements.size(), 1u);
    EXPECT_EQ(value.Value.Fields.size(), 2u);
    const auto compiled = CompileConstructor(value, loaded);
    EXPECT_EQ(compiled.Parameters[0].OptionCppType, "std::vector<::TItem>");
    EXPECT_EQ(compiled.Parameters[2].When->Dependencies, std::vector<std::size_t>{1});
    EXPECT_EQ(compiled.Requirements[0].Dependencies, (std::vector<std::size_t>{1, 2}));
}

TEST(ConstructorTest, DefaultPoolAndWhenDependenciesRemainSeparate) {
    const auto loaded = Rules(R"(
        TActor Fighter(bool Enable = true, TPool Choices = Pool,
            choice Feature Value from Choices.Positive(1).Values when Enable = First) {
            Features = [Value];
        }
    )");
    const auto compiled = CompileConstructor(Last(loaded), loaded);
    EXPECT_EQ(compiled.Parameters[2].When->Dependencies, std::vector<std::size_t>{0});
    EXPECT_EQ(compiled.Parameters[2].Pool->Dependencies, std::vector<std::size_t>{1});
    EXPECT_TRUE(compiled.Parameters[2].Default->Dependencies.empty());
    EXPECT_NO_THROW(Implementation(loaded));
}

TEST(ConstructorTest, OldFactoriesKeepTheirSignatures) {
    const auto loaded = Rules("TActor Wolf { Level = 2; }");
    EXPECT_FALSE(Last(loaded).Parameterized);
}

TEST(ConstructorTest, SupportsRequiredOnlyAndEmptyParameterLists) {
    const auto loaded = Rules("TActor Wolf() { requires true; } TActor Fighter(int Level) { Level = Level; }");
    EXPECT_NO_THROW(Header(loaded));
    EXPECT_NO_THROW(Implementation(loaded));
}

TEST(ConstructorTest, FeatureEqualityAndDirectMembershipAreTypedPureOperations) {
    const auto loaded = Rules(R"(
        TActor Fighter(collection<Feature> Selected = [First], Feature Value = First) {
            requires Value == First;
            requires First != Second || Selected.has(First);
            Included = Selected.has(Value);
            Features = Selected;
        }
    )");
    EXPECT_NO_THROW(Implementation(loaded));
}

TEST(ConstructorTest, RejectsForwardNamesBeforeGlobalFallback) {
    for (const auto& source : {
        "TActor Bad(int Earlier = Later, int Later = 1) {}",
        "TActor Bad(choice Feature Earlier from [First] = First, Feature First = Second) {}",
        "TActor Bad(choice Feature Earlier from Pool.Values when Later = First, bool Later = true) {}",
        "TActor Bad(choice Feature Earlier from Later.Values = First, TPool Later = Pool) {}"
    }) {
        SCOPED_TRACE(source);
        EXPECT_THROW(Header(Rules(source)), std::runtime_error);
    }
}

TEST(ConstructorTest, RejectsWrongTypesAndInvalidParameterDeclarations) {
    for (const auto& source : {
        "TActor Bad(choice int Value from [First] = 1) {}",
        "TActor Bad(collection<int> Values = [1]) {}",
        "TActor Bad(TActor Value) {}",
        "TActor Bad(int Value when true = 1) {}",
        "TActor Bad(int Value = First) {}",
        "TActor Bad(choice Feature Value from [Blade] = First) {}",
        "TActor Bad(choice Feature Value from Pool.Values when 1 = First) {}",
        "TActor Bad(int Value = 1, int Value = 2) {}",
        "TActor Bad(int class = 1) {}",
        "TActor Bad() { requires 1; }",
        "TActor Bad(collection<Feature> Values = [First]) { Items = Values; }",
        "TActor Bad(collection<Feature> Values = [First]) { Included = Values.has(Blade); }"
    }) {
        SCOPED_TRACE(source);
        EXPECT_THROW(Header(Rules(source)), std::runtime_error);
    }
}

TEST(ConstructorTest, RejectsImpureAndEntityConstructingInputs) {
    for (const auto& source : {
        "TActor Bad(int Value = @self.Level) {}",
        "TActor Bad(int Value = roll(1, 6)) {}",
        "TActor Bad(int Value = TActor {}.Level) {}",
        "TActor Bad() { Level = @self.Level; }",
        "TActor Bad() { requires same_team(@self, @self); }"
    }) {
        SCOPED_TRACE(source);
        EXPECT_THROW(Header(Rules(source)), std::runtime_error);
    }
}

TEST(ConstructorTest, RejectsNamedCyclesThroughParameterizedDefaultsAndQueries) {
    for (const auto& source : {
        "TItem Loop(TItem Value = Loop) { Value = Value.Value; }",
        "TPool Loop(choice Feature Value from Loop.Values = First) { Values = [Value]; }",
        "TPool Loop(bool Enable = Loop.Contains(First)) {}",
        "TPool Loop() { requires Loop.Contains(First); }",
        "class TReader { query int Read() { return Loop.Value; } } TItem Loop(int Value = TReader {}.Read()) { Value = Value; }",
        "class TDefault { int Value = Loop.Value; } TDefault Loop(int Unused = 1) {}"
    }) {
        SCOPED_TRACE(source);
        EXPECT_THROW(Header(Rules(source)), std::runtime_error);
    }
}

TEST(ConstructorTest, ParameterizedNamedValuesCanBeReusedWithoutArguments) {
    const auto loaded = Rules("TItem Configured(int Amount = 5) { Value = Amount; } TActor Fighter(collection<TItem> Items = [Configured, Configured]) { Items = Items; }");
    EXPECT_NO_THROW(Implementation(loaded));
}

TEST(ConstructorTest, RejectsGeneratedOptionsResolverAndDescriptorNameCollisions) {
    for (const auto& name : {"TFighterOptions", "TResolvedFighter", "ResolveFighter", "DescribeFighter", "ValidateFighter", "CreateFighter"}) {
        SCOPED_TRACE(name);
        EXPECT_THROW(Header(Rules("class " + std::string(name) + " {} TActor Fighter(int Value = 1) {}")), std::runtime_error);
    }
    EXPECT_THROW(Header(Rules("class TConstructionReport {} TActor Fighter() {}")), std::runtime_error);
}

TEST(ConstructorTest, ParameterNamesAndGeneratedBlankTypesRemainUnambiguous) {
    EXPECT_THROW(Header(Rules("TActor Bad(int Earlier = min_int, int min_int = 1) {}")), std::runtime_error);
}

TEST(ConstructorTest, MemberNamesDoNotTerminateConstructorParameterExpressions) {
    const auto loaded = Rules(R"(
        class TKeywords { collection<Feature> when; }
        TKeywords Keywords { when = [First]; }
        TActor Fighter(choice Feature Value from Keywords.when = First) { Features = [Value]; }
    )");
    EXPECT_NO_THROW(Header(loaded));
}

