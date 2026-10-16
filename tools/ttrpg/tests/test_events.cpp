#include <gtest/gtest.h>

#include <ttrpg/emit.h>
#include <ttrpg/events.h>
#include <ttrpg/parser.h>
#include <ttrpg/program.h>

#include <sstream>
#include <stdexcept>
#include <string>

namespace {

const std::string kTypes = R"(
    enum EMode { Low, High }
    class TValue {
        int Amount;
        query int Bonus() { return @self.Amount + 1; }
    }
    class TDice { int Count; int Size = 6; }
    GameEntity TActor {
        int Level = 1;
        Resource Effort = 3;
        method void Improve(int Amount) { @self.Level += Amount; }
        query bool IsAlive() { return @self.Level > 0; }
    }
    GameEntity TDevice { int Charges = 2; }
)";

TLoadedSchemas Rules(const std::string& source) {
    TLoadedSchemas loaded;
    loaded.PrimaryStem = "feature_events";
    loaded.Primary = TParser(Tokenize(kTypes + source)).Parse();
    RegisterModuleSymbols(loaded.PrimaryStem, loaded.Primary, loaded.SymbolTable);
    return loaded;
}

std::string Header(const TLoadedSchemas& loaded) {
    std::ostringstream result;
    EmitHeader(result, loaded, "feature_events.ttrpg", "feature_events.h");
    return result.str();
}

std::string Implementation(const TLoadedSchemas& loaded) {
    std::ostringstream result;
    EmitImpl(result, loaded, "feature_events.ttrpg", "feature_events.h");
    return result.str();
}

std::string Join(const std::vector<std::string>& lines) {
    std::string result;
    for (const auto& line : lines) result += line + "\n";
    return result;
}

void Rejects(const std::string& source, const std::string& diagnostic) {
    try {
        Implementation(Rules(source));
        FAIL() << "expected rejection: " << source;
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find(diagnostic), std::string::npos) << error.what();
    }
}

}

TEST(FeatureEventCompilerTest, ParsesTypedEventsAndGrantlessFeatureHandlers) {
    const auto loaded = Rules(R"(
        event int HealingBonus for TActor(TActor Target, int Amount);
        action External;
        feature Training {
            grant External;
            on HealingBonus { return @self.Level + Target.Level + Amount; }
        }
        feature Helpful { on HealingBonus { return 2; } }
    )");
    ASSERT_EQ(loaded.Primary.Events.size(), 1u);
    const auto& event = loaded.Primary.Events.front();
    EXPECT_EQ(event.Name, "HealingBonus");
    EXPECT_EQ(event.OwnerType, "TActor");
    EXPECT_EQ(event.ReturnType, "int");
    ASSERT_EQ(event.Parameters.size(), 2u);
    ASSERT_EQ(loaded.Primary.Abilities[1].Hooks.size(), 1u);
    EXPECT_EQ(loaded.Primary.Abilities[1].Hooks[0].EventName, "HealingBonus");
    EXPECT_TRUE(loaded.Primary.Abilities[2].Grants.empty());
    const auto header = Header(loaded);
    EXPECT_NE(header.find("struct TEventHealingBonus"), std::string::npos);
    EXPECT_NE(header.find("using TArguments = std::tuple<TGeneratedEntityBinding<TActor>, int>;"), std::string::npos);
    EXPECT_NE(header.find("using FHandler = std::function<TResult(const TGeneratedWorldView&, const TGeneratedEntityBinding<TOwner>&, const TArguments&)>;"), std::string::npos);
    EXPECT_NE(header.find("kId = \"feature_events.HealingBonus\""), std::string::npos);
}

TEST(FeatureEventCompilerTest, CompilesReadOnlyHookParametersQueriesAndClassResults) {
    const auto loaded = Rules(R"(
        event TDice ExtraDice for TActor(TActor Target, TValue Configuration, EMode Mode);
        feature Training {
            on ExtraDice {
                let Alias = Target;
                if (@self.IsAlive() && Alias.IsAlive() && Mode == High) {
                    return TDice { Count = Configuration.Bonus(); Size = 4; };
                }
                return TDice {};
            }
        }
    )");
    const auto& feature = loaded.Primary.Abilities.front();
    const auto body = Join(CompileFeatureHook(feature, feature.Hooks.front(), loaded));
    EXPECT_NE(body.find("const auto& arg_Target = std::get<0>(arguments)"), std::string::npos);
    EXPECT_NE(body.find("auto local_Alias = arg_Target"), std::string::npos);
    EXPECT_NE(body.find("action_self.Read()"), std::string::npos);
    EXPECT_NE(body.find("local_Alias.Read()"), std::string::npos);
    EXPECT_NE(body.find("EMode::High"), std::string::npos);
    EXPECT_NE(body.find("TDice::Create({.Count = (arg_Configuration).Bonus(), .Size = 4})"), std::string::npos);
    EXPECT_EQ(body.find("ctx"), std::string::npos);
    EXPECT_EQ(body.find("Guard()"), std::string::npos);
    EXPECT_EQ(body.find("transaction"), std::string::npos);
    const auto implementation = Implementation(loaded);
    EXPECT_NE(implementation.find("std::make_shared<const TEventExtraDice::FHandler>"), std::string::npos);
    EXPECT_NE(implementation.find("rules.Events.Register<TEventExtraDice>(\"feature_events.Training\", hook_0)"), std::string::npos);
}

TEST(FeatureEventCompilerTest, CopiesContributionValuesAndPreservesOuterLocalsAcrossChoice) {
    const auto loaded = Rules(R"(
        event int HealingBonus for TActor(TActor Target, int Amount);
        action Mend for TActor {
            choose TActor First;
            let Amount = 1;
            let Bonuses = HealingBonus(@self, First, Amount);
            for Bonus in Bonuses {
                Bonus += 1;
                Amount += Bonus;
            }
            choose TActor Second;
            Second.Improve(Amount);
            for Bonus in Bonuses { @self.Improve(Bonus); }
        }
    )");
    const auto body = Join(CompileAction(loaded.Primary.Abilities.front(), loaded).Body);
    EXPECT_NE(body.find("DispatchFeatureEvent<::TEventHealingBonus>(Context, action_self, ::TEventHealingBonus::TArguments{bound_First, local_Amount})"), std::string::npos);
    EXPECT_NE(body.find("const auto event_values_0 = local_Bonuses;"), std::string::npos);
    EXPECT_NE(body.find("auto local_Bonus = event_value_0.Value;"), std::string::npos);
    EXPECT_NE(body.find("local_Amount = TRuleInteger::Add(local_Amount, local_Bonus)"), std::string::npos);
    EXPECT_NE(body.find("auto& local_Amount = frame.slot_1.value()"), std::string::npos);
    EXPECT_NE(body.find("std::optional<std::vector<TFeatureEventContribution<int>>> slot_2"), std::string::npos);
    EXPECT_EQ(body.find("auto& local_Bonus = frame."), std::string::npos);
    EXPECT_NE(body.find("const auto event_values_1 = local_Bonuses;"), std::string::npos);
}

TEST(FeatureEventCompilerTest, RollsClassContributionsExactlyOncePerLoopIteration) {
    const auto loaded = Rules(R"(
        event TDice ExtraDice for TActor(TActor Target);
        action Mend for TActor {
            choose TActor Target;
            let Amount = 0;
            for Dice in ExtraDice(@self, Target) {
                Amount += roll(Dice.Count, Dice.Size);
            }
            Target.Improve(Amount);
        }
    )");
    const auto body = Join(CompileAction(loaded.Primary.Abilities.front(), loaded).Body);
    const auto roll = body.find("RollActionDice(");
    ASSERT_NE(roll, std::string::npos);
    EXPECT_EQ(body.find("RollActionDice(", roll + 1), std::string::npos);
    EXPECT_NE(body.find("(static_cast<const ::TDice&>(local_Dice)).Count()"), std::string::npos);
    EXPECT_NE(body.find("local_Amount = TRuleInteger::Add(local_Amount, RollActionDice("), std::string::npos);
}

TEST(FeatureEventCompilerTest, SupportsPrimitiveEnumAndOrdinaryClassEventArguments) {
    const auto loaded = Rules(R"(
        event EMode PickMode for TActor(string Label, bool Enabled, TValue Configuration);
        feature Training { on PickMode { if (Enabled) { return High; } return Low; } }
        action Inspect for TActor {
            for Mode in PickMode(@self, "test", true, TValue { Amount = 1; }) {
                if (Mode == High) { @self.Improve(1); }
            }
        }
    )");
    const auto implementation = Implementation(loaded);
    EXPECT_NE(implementation.find("TEventPickMode::TArguments{std::string(\"test\"), true, ::TValue::Create({.Amount = 1})}"), std::string::npos);
    EXPECT_NE(Header(loaded).find("using TResult = EMode;"), std::string::npos);
}

TEST(FeatureEventCompilerTest, HandlesImportedEventsAndEscapesEventIds) {
    auto loaded = Rules("feature Training { on Imported { return Amount; } }");
    loaded.Primary.Imports.push_back("events.ttrpg");
    loaded.ByStem["events"] = TParser(Tokenize("event int Imported for TActor(int Amount);")).Parse();
    EXPECT_NE(Implementation(loaded).find("rules.Events.Register<TEventImported>"), std::string::npos);
    EXPECT_NE(Header(loaded).find("#include <events.h>"), std::string::npos);
    auto escaped = Rules("event int Bonus for TActor();");
    escaped.PrimaryStem = "odd\"event\\module\n";
    EXPECT_NE(Header(escaped).find("\"odd\\\"event\\\\module\\n.Bonus\""), std::string::npos);
}

TEST(FeatureEventCompilerTest, RejectsConflictingOrUnsupportedDeclarations) {
    Rejects("event int Bonus for TActor(); event int Bonus for TActor();", "duplicate event");
    Rejects("event int TValue for TActor();", "conflicts with existing symbol");
    Rejects("action Bonus; event int Bonus for TActor();", "conflicts with existing symbol");
    Rejects("class TEventBonus {} event int Bonus for TActor();", "conflicts with existing symbol");
    Rejects("event int roll for TActor();", "conflicts with existing symbol");
    Rejects("event int Bonus for TValue();", "owner must be a GameEntity");
    Rejects("event TActor Bonus for TActor();", "result must be");
    Rejects("event void Bonus for TActor();", "result must be");
    Rejects("event int Bonus for TActor(Resource Pool);", "unsupported event parameter type");
    Rejects("event int Bonus for TActor(int Amount, bool Amount);", "duplicate event parameter");
    Rejects("feature Training { on Missing { return 1; } }", "unknown hook event");
    Rejects("event int Bonus for TActor(); feature Training { on Bonus { return 1; } on Bonus { return 2; } }", "duplicate feature hook");
}

TEST(FeatureEventCompilerTest, RejectsImpureOrIncompleteHandlers) {
    const std::string prefix = "event int Bonus for TActor(TActor Target); feature Training { on Bonus { ";
    Rejects(prefix + "@self.Level = 2; return 1; } }", "query cannot modify");
    Rejects(prefix + "Target.Improve(2); return 1; } }", "pure expression");
    Rejects(prefix + "return roll(1, 6); } }", "effectful action expression");
    Rejects(prefix + "let Values = Bonus(@self, Target); return 1; } }", "not allowed inside a feature hook");
    Rejects(prefix + "choose TActor Other; return 1; } }", "synchronous scope");
    Rejects(prefix + "if (Target.IsAlive()) { return 1; } } }", "every path");
    Rejects(prefix + "return true; } }", "expected 'int'");
}

TEST(FeatureEventCompilerTest, RejectsBadInvocationAndLoopScopes) {
    const std::string prefix = "event int Bonus for TActor(TActor Target, int Amount); action A for TActor { ";
    Rejects(prefix + "let Values = Bonus(@self); }", "wrong argument count");
    Rejects(prefix + "let Values = Bonus(1, @self, 2); }", "expected 'TActor'");
    Rejects(prefix + "choose TDevice Target; let Values = Bonus(@self, Target, 2); }", "expected 'TActor'");
    Rejects(prefix + "for Value in Bonus(@self, @self, true) {} }", "expected 'int'");
    Rejects(prefix + "for Value in 2 {} }", "contribution sequence");
    Rejects(prefix + "for Value in Bonus(@self, @self, 2) { choose TActor Target; } }", "synchronous scope");
    Rejects(prefix + "let Value = 2; for Value in Bonus(@self, @self, 2) {} }", "duplicate local");
    Rejects(prefix + "for Value in Bonus(@self, @self, 2) {} @self.Improve(Value); }", "unknown name");
    Rejects("event int Bonus for TActor(); class TBad { query int Value() { let Values = Bonus(@self); return 1; } }", "action context");
}

TEST(FeatureEventCompilerTest, LoopReturnDoesNotHideStatementsAfterAnEmptySequence) {
    const auto loaded = Rules(R"(
        event int Bonus for TActor();
        action A for TActor {
            for Value in Bonus(@self) { return; }
            @self.Improve(2);
        }
    )");
    const auto body = Join(CompileAction(loaded.Primary.Abilities.front(), loaded).Body);
    EXPECT_NE(body.find("return;"), std::string::npos);
    EXPECT_NE(body.find("&::TActor::Improve, 2"), std::string::npos);
}
