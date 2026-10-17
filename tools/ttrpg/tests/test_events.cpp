#include <gtest/gtest.h>

#include "compile_helpers.h"

#include <ttrpg/events.h>
#include <ttrpg/parser.h>
#include <ttrpg/program.h>

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
    return LoadRules("feature_events", kTypes + source);
}

std::string Header(const TLoadedSchemas& loaded) {
    return EmitHeaderText(loaded, "feature_events");
}

std::string Implementation(const TLoadedSchemas& loaded) {
    return EmitImplText(loaded, "feature_events");
}

void Rejects(const std::string& source, const std::string& diagnostic) {
    ExpectRejected(source, diagnostic, [&] { Implementation(Rules(source)); });
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
