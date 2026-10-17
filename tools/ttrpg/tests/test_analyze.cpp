#include <gtest/gtest.h>

#include <ttrpg/parser.h>
#include <ttrpg/program.h>
#include <ttrpg/schema_ast.h>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

TSchemaModule ParseModule(const std::string& src) {
    return TParser(Tokenize(src)).Parse();
}

TLoadedSchemas RuleModule(const std::string& source) {
    TLoadedSchemas loaded;
    loaded.Primary = ParseModule(source);
    loaded.PrimaryStem = "rules";
    loaded.ByStem.emplace("rules", loaded.Primary);
    RegisterModuleSymbols("rules", loaded.Primary, loaded.SymbolTable);
    return loaded;
}

void CheckRules(const std::string& source) {
    const auto loaded = RuleModule(source);
    for (const auto& declaration : loaded.Primary.Classes) CompileClass(declaration, loaded);
}

}

TEST(RuleAnalyzeTest, StoredInitializationAndDerivedValues) {
    const auto loaded = RuleModule(R"(
        class TPoints {
            int Current = @self.Maximum;
            int Maximum = 10;
            derive bool Empty = @self.Current == 0;
        }
    )");
    const auto compiled = CompileClass(loaded.Primary.Classes[0], loaded);
    EXPECT_EQ(compiled.InitOrder, (std::vector<std::size_t>{1, 0}));
    EXPECT_FALSE(compiled.Fields[2].Initializer.empty());
}

TEST(RuleAnalyzeTest, RejectsMutationInQueryAndDerivedFields) {
    EXPECT_THROW(CheckRules(R"(
        class TPoints {
            int Current = 10;
            query void Hurt() { @self.Current -= 1; }
        }
    )"), std::runtime_error);
    EXPECT_THROW(CheckRules(R"(
        class TPoints {
            int Current = 10;
            derive int Twice = @self.Current * 2;
            method void Hurt() { @self.Twice = 1; }
        }
    )"), std::runtime_error);
    EXPECT_THROW(CheckRules(R"(
        class TPoints {
            int Current = 10;
            derive int Bad = @self.Hurt();
            method int Hurt() { @self.Current -= 1; return @self.Current; }
        }
    )"), std::runtime_error);
}

TEST(RuleAnalyzeTest, RejectsInvalidArgumentsAndReturnPaths) {
    EXPECT_THROW(CheckRules(R"(
        class TPoints {
            method int Heal(int Amount) { return Amount; }
            method void Hurt() { @self.Heal(true); }
        }
    )"), std::runtime_error);
    EXPECT_THROW(CheckRules(R"(
        class TPoints {
            method int Heal(int Amount) { if (Amount > 0) { return Amount; } }
        }
    )"), std::runtime_error);
    EXPECT_THROW(CheckRules(R"(
        class TPoints {
            method int Heal(int Amount) { return Amount; }
            method void Hurt() { @self.Heal(); }
        }
    )"), std::runtime_error);
}

TEST(RuleAnalyzeTest, DetectsDerivedCycles) {
    EXPECT_THROW(CheckRules(R"(
        class TPoints {
            derive int A = @self.B + 1;
            derive int B = @self.A + 1;
        }
    )"), std::runtime_error);
}

TEST(RuleAnalyzeTest, ValidatesObjectInitializers) {
    EXPECT_THROW(CheckRules(R"(
        class TResult { int Value; derive int Twice = @self.Value * 2; }
        class TPoints {
            method TResult Get() { return TResult { Twice = 1; }; }
        }
    )"), std::runtime_error);
    EXPECT_NO_THROW(CheckRules(R"(
        class TResult { int Value; derive int Twice = @self.Value * 2; }
        class TPoints {
            int Current = 10;
            method TResult Hurt(int Amount) {
                require Amount >= 0;
                let Before = @self.Current;
                @self.Current = max(0, Before - Amount);
                return TResult { Value = Before - @self.Current; };
            }
        }
    )"));
}

TEST(RuleAnalyzeTest, QueryDependenciesParticipateInInitialization) {
    const auto loaded = RuleModule(R"(
        class TPoints {
            int Current = @self.ReadMaximum();
            int Maximum = 10;
            query int ReadMaximum() { return @self.Maximum; }
        }
    )");
    EXPECT_EQ(CompileClass(loaded.Primary.Classes[0], loaded).InitOrder,
              (std::vector<std::size_t>{1, 0}));
    EXPECT_THROW(CheckRules(R"(
        class TPoints {
            derive int Current = @self.ReadCurrent();
            query int ReadCurrent() { return @self.Current; }
        }
    )"), std::runtime_error);
    EXPECT_THROW(CheckRules(R"(
        class TPoints {
            query int A() { return @self.B(); }
            query int B() { return @self.A(); }
        }
    )"), std::runtime_error);
}

TEST(RuleAnalyzeTest, RejectsEntityValuesAndCppKeywordMembers) {
    EXPECT_THROW(CheckRules(R"(
        GameEntity TChild { int Value; }
        GameEntity TParent { TChild Child; }
    )"), std::runtime_error);
    EXPECT_THROW(CheckRules(R"(
        class TPoints { int class = 1; query int Read() { return @self.class; } }
    )"), std::runtime_error);
}

TEST(RuleAnalyzeTest, PlainValueClassesSupportAggregateConstruction) {
    EXPECT_NO_THROW(CheckRules(R"(
        class TResult { int Value; }
        class TPoints { method TResult Read() { return TResult { Value = 1; }; } }
    )"));
    EXPECT_THROW(CheckRules(R"(
        enum EKey { A, B, }
        class TResult { set<EKey> Keys; }
        class TPoints { method TResult Read() { return TResult { Keys = A; }; } }
    )"), std::runtime_error);
}

TEST(RuleAnalyzeTest, IntegerLiteralsAreDecimalInGeneratedCpp) {
    const auto loaded = RuleModule(R"(
        class TPoints { int Value = 010; query int Read() { return 08; } }
    )");
    const auto compiled = CompileClass(loaded.Primary.Classes[0], loaded);
    EXPECT_EQ(compiled.Fields[0].Initializer, "10");
    EXPECT_NE(std::find(compiled.Methods[0].Body.begin(), compiled.Methods[0].Body.end(),
                        "auto result = 8;"), compiled.Methods[0].Body.end());
}

TEST(RuleAnalyzeTest, InvalidContainersAndGeneratedMemberCollisionsAreDiagnosed) {
    EXPECT_THROW(CheckRules("class TPoints { set<int> Values; }"), std::runtime_error);
    EXPECT_THROW(CheckRules("class TPoints { collection<int> Values; }"), std::runtime_error);
    EXPECT_THROW(CheckRules("class TPoints { map<int, int> Values; }"), std::runtime_error);
    EXPECT_THROW(CheckRules("class TPoints { int TPoints; }"), std::runtime_error);
    EXPECT_THROW(CheckRules("class TPoints { int Value; int Value_; }"), std::runtime_error);
}

TEST(RuleAnalyzeTest, QueriesCannotAllocateEntityIdentities) {
    EXPECT_THROW(CheckRules(R"(
        GameEntity TActor { int Value; }
        class TFactory { query TActor Make() { return TActor { Value = 1; }; } }
    )"), std::runtime_error);
    EXPECT_THROW(CheckRules(R"(
        GameEntity TActor { int Value; }
        class TFactory { method TActor Make() { return TActor { Value = 1; }; } }
    )"), std::runtime_error);
}

TEST(RuleAnalyzeTest, EnumParametersAndEqualityUseDeclaredTypes) {
    const auto loaded = RuleModule(R"(
        enum ERank { None, Basic, Advanced, }
        class TRating {
            ERank Rank = None;
            query bool Matches(ERank Expected) { return @self.Rank == Expected; }
            query bool IsAdvanced() { return @self.Rank != Basic && Advanced == @self.Rank; }
            query ERank Echo(ERank Value) { return Value; }
            method void Set(ERank Value) { @self.Rank = Value; }
            query bool ReadsEnumResult() { return @self.Echo(Advanced) == Advanced; }
        }
    )");
    const auto compiled = CompileClass(loaded.Primary.Classes[0], loaded);
    EXPECT_EQ(compiled.Fields[0].Initializer, "ERank::None");
    const auto& body = compiled.Methods[1].Body;
    const auto result = std::find_if(body.begin(), body.end(), [](const auto& line) {
        return line.find("ERank::Basic") != std::string::npos && line.find("ERank::Advanced") != std::string::npos;
    });
    EXPECT_NE(result, body.end());
}

TEST(RuleAnalyzeTest, EnumLiteralsResolveAgainstContextAndReportAmbiguity) {
    EXPECT_NO_THROW(CheckRules(R"(
        enum EFirst { Shared, FirstOnly, }
        enum ESecond { Shared, SecondOnly, }
        class TRating {
            EFirst First = Shared;
            ESecond Second = Shared;
            query bool Check(EFirst Value) { return Shared == Value && @self.Second == Shared; }
            query ESecond Selected() { return Shared; }
            query bool Unique() { let Value = FirstOnly; return Value == @self.First; }
        }
    )"));
    EXPECT_THROW(CheckRules(R"(
        enum EFirst { Shared, }
        enum ESecond { Shared, }
        class TRating { query bool Check() { let Value = Shared; return true; } }
    )"), std::runtime_error);
    const auto loaded = RuleModule("enum ERank { Basic, } class TRating { ERank Rank = Basic; }");
    auto primary_only = loaded;
    primary_only.ByStem.clear();
    EXPECT_EQ(CompileClass(primary_only.Primary.Classes[0], primary_only).Fields[0].Initializer, "ERank::Basic");
}

TEST(RuleAnalyzeTest, RejectsMixedEnumTypesAndNonEqualityOperators) {
    for (const std::string expression : {"Left == Right", "Left != Right", "Left == 0", "Left < FirstOnly", "Left + 1"}) {
        EXPECT_THROW(CheckRules(
            "enum EFirst { FirstOnly, } enum ESecond { SecondOnly, } "
            "class TRating { query bool Check(EFirst Left, ESecond Right) { return " + expression + "; } }"),
            std::runtime_error) << expression;
    }
    EXPECT_THROW(CheckRules(R"(
        enum EFirst { FirstOnly, }
        enum ESecond { SecondOnly, }
        class TRating {
            query bool Accept(EFirst Value) { return true; }
            query bool Check(ESecond Value) { return @self.Accept(Value); }
        }
    )"), std::runtime_error);
    EXPECT_THROW(CheckRules("GameEntity TValue {} class TReader { query int Read(TValue Value) { return 1; } }"), std::runtime_error);
}

TEST(RuleAnalyzeTest, MapGetSupportsPrimitiveAndEnumValues) {
    EXPECT_NO_THROW(CheckRules(R"(
        enum EKey { One, Two, }
        enum ERank { None, Basic, }
        class TReader {
            map<EKey, int> Numbers;
            map<EKey, bool> Flags;
            map<EKey, string> Names;
            map<EKey, ERank> Ranks;
            query int Number(EKey Key) { return @self.Numbers.get(Key, 3); }
            query bool Flag(EKey Key) { return @self.Flags.get(Key, true); }
            query string Name(EKey Key) { return @self.Names.get(Key, "Unknown"); }
            query ERank Rank(EKey Key) { return @self.Ranks.get(Key, None); }
            query bool IsBasic(EKey Key) { return @self.Ranks.get(Key, None) == Basic; }
        }
    )"));
}

TEST(RuleAnalyzeTest, MapGetValidatesKeyFallbackReceiverAndArity) {
    for (const std::string expression : {
             "@self.Values.get(1, TValue {})", "@self.Values.get(OtherKey, TValue {})",
             "@self.Values.get(Key, 0)", "@self.Values.get(Key, TOther {})",
             "@self.Values.get(Key)", "@self.Values.get(Key, TValue {}, TValue {})",
             "@self.Values.find(Key)", "@self.Values[Key]", "@self.Number.get(Key, 0)"}) {
        EXPECT_THROW(CheckRules(
            "enum EKey { Key, } enum EOther { OtherKey, } "
            "class TValue {} class TOther {} "
            "class TReader { map<EKey, TValue> Values; int Number; query TValue Read() { return " + expression + "; } }"),
            std::runtime_error) << expression;
    }
}

TEST(RuleAnalyzeTest, MapGetCannotMutateArgumentsOrReturnedValues) {
    EXPECT_THROW(CheckRules(R"(
        enum EKey { One, }
        class TValue { int Amount; method void Change() { @self.Amount = 4; } }
        class TReader {
            map<EKey, TValue> Values;
            method void Change() { @self.Values.get(One, TValue {}).Change(); }
        }
    )"), std::runtime_error);
    EXPECT_THROW(CheckRules(R"(
        enum EKey { One, }
        class TValue { int Amount; }
        class TReader {
            map<EKey, TValue> Values;
            method void Change() { @self.Values.get(One, TValue {}).Amount = 2; }
        }
    )"), std::runtime_error);
    EXPECT_THROW(CheckRules(R"(
        enum EKey { One, }
        class TReader {
            map<EKey, int> Values;
            int Number;
            method int Change() { @self.Number += 1; return @self.Number; }
            method int Read() { return @self.Values.get(One, @self.Change()); }
        }
    )"), std::runtime_error);
    EXPECT_THROW(CheckRules(R"(
        enum EKey { One, }
        class TReader {
            map<EKey, int> Values;
            method EKey Change() { return One; }
            method int Read() { return @self.Values.get(@self.Change(), 0); }
        }
    )"), std::runtime_error);
}

TEST(RuleAnalyzeTest, MapGetDependenciesParticipateInInitializationAndCycleDetection) {
    const auto loaded = RuleModule(R"(
        enum EKey { One, }
        class TReader {
            int Result = @self.Read();
            map<EKey, int> Values;
            EKey Key = One;
            int Fallback = 3;
            query int Read() { return @self.Values.get(@self.Key, @self.Fallback); }
        }
    )");
    EXPECT_EQ(CompileClass(loaded.Primary.Classes[0], loaded).InitOrder,
              (std::vector<std::size_t>{1, 2, 3, 0}));
    EXPECT_THROW(CheckRules(R"(
        enum EKey { One, }
        class TReader {
            derive int Result = @self.Read();
            map<EKey, int> Values;
            query int Read() { return @self.Values.get(One, @self.Result); }
        }
    )"), std::runtime_error);
}

TEST(RuleAnalyzeTest, VoidReturnCallsDoNotDeclareVoidVariables) {
    const auto loaded = RuleModule(R"(
        class TPoints {
            method void Noop() { return; }
            method void Execute() { return @self.Noop(); }
        }
    )");
    const auto compiled = CompileClass(loaded.Primary.Classes[0], loaded);
    for (const auto& line : compiled.Methods[1].Body) {
        EXPECT_EQ(line.find("auto result"), std::string::npos);
    }
}

TEST(RuleAnalyzeTest, MapGetReturnsClassValuesForTemporaryAndLocalQueries) {
    const auto loaded = RuleModule(R"(
        enum ECategory { Light, Heavy, }
        class TValue {
            int Amount = 1;
            query int Plus(int Value) { return @self.Amount + Value; }
        }
        class TReader {
            map<ECategory, TValue> Values;
            query TValue Lookup(ECategory Key) { return @self.Values.get(Key, TValue {}); }
            query int Direct(ECategory Key) { return @self.Values.get(Key, TValue {}).Plus(2); }
            query int ThroughLocal(ECategory Key) {
                let Selected = @self.Values.get(Key, TValue { Amount = 4; });
                return Selected.Plus(2);
            }
            query int TemporaryField(ECategory Key) { return @self.Values.get(Key, TValue {}).Amount; }
            query int CopiedMap(ECategory Key) {
                let Copied = @self.Values;
                return Copied.get(Key, TValue {}).Amount;
            }
        }
    )");
    const auto compiled = CompileClass(loaded.Primary.Classes[1], loaded);
    std::string body;
    for (const auto& method : compiled.Methods) {
        for (const auto& line : method.Body) body += line + "\n";
    }
    EXPECT_NE(body.find("rule_map.find(rule_key)"), std::string::npos);
    EXPECT_NE(body.find("-> TValue"), std::string::npos);
    EXPECT_NE(body.find("static_cast<const TValue&>"), std::string::npos);
    EXPECT_NE(body.find("}(local_Copied, arg_Key, TValue::Create({}))"), std::string::npos);
    EXPECT_EQ(body.find("ctx."), std::string::npos);
    EXPECT_EQ(body.find("rule_map["), std::string::npos);
}
