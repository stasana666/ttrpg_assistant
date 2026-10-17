#include <gtest/gtest.h>

#include "compile_helpers.h"

#include <ttrpg/abilities.h>
#include <ttrpg/emit.h>
#include <ttrpg/parser.h>
#include <ttrpg/program.h>

#include <sstream>
#include <stdexcept>
#include <string>

namespace {

const std::string kTypes = R"(
    class THealth {
        int Current = 10;
        method void Heal(int Amount) { @self.Current += Amount; }
        query bool IsPositive() { return @self.Current > 0; }
    }
    class TPool { Resource Energy = 3; }
    GameEntity TActor {
        Resource Effort = 3;
        TPool Pool;
        THealth Health;
        int Level = 1;
        query bool IsAlive() { return @self.Health.IsPositive(); }
    }
    GameEntity TDevice { int Charges = 2; }
)";

TLoadedSchemas Rules(const std::string& source) {
    return LoadRules("compiled_actions", kTypes + source);
}

std::string Implementation(const TLoadedSchemas& loaded) {
    return EmitImplText(loaded, "compiled_actions");
}

std::string Body(const TCompiledAction& action) {
    std::string result;
    for (const auto& line : action.Body) result += line + "\n";
    return result;
}

void Rejects(const std::string& source, const std::string& diagnostic) {
    ExpectRejected(source, diagnostic, [&] { Implementation(Rules(source)); });
}

}

TEST(ActionCompilerTest, ParsesTypedBodiesAlongsideAbilityDeclarations) {
    const auto loaded = Rules(R"(
        action External;
        reaction Opportunity;
        action Mend for TActor {
            cost @self.Effort = 1;
            choose TActor Target where Target.IsAlive();
            let Amount = roll(1, 6);
            Target.Health.Heal(Amount);
        }
        feature Training { grant Mend; }
    )");
    ASSERT_EQ(loaded.Primary.Abilities.size(), 4u);
    EXPECT_TRUE(loaded.Primary.Abilities[0].OwnerType.empty());
    const auto& action = loaded.Primary.Abilities[2];
    EXPECT_EQ(action.OwnerType, "TActor");
    ASSERT_EQ(action.Body.size(), 4u);
    EXPECT_EQ(action.Body[0].Kind, TClassDecl::TStatement::EKind::Cost);
    EXPECT_EQ(action.Body[1].Kind, TClassDecl::TStatement::EKind::Choose);
    EXPECT_EQ(action.Body[1].TypeName, "TActor");
    EXPECT_EQ(action.Body[1].Name, "Target");
    ASSERT_TRUE(action.Body[1].Value.has_value());
    const auto compiled = CompileAction(action, loaded);
}

TEST(ActionCompilerTest, BindsArbitraryStoredResourcePathsAndConstantCosts) {
    const auto loaded = Rules(R"(
        action Channel for TActor {
            cost @self.Effort = max(1, 2 * 2 - 1);
            cost @self.Pool.Energy = 4 / 2;
            @self.Health.Heal(2);
        }
    )");
    const auto compiled = CompileAction(loaded.Primary.Abilities.front(), loaded);
    ASSERT_EQ(compiled.Costs.size(), 2u);
    EXPECT_EQ(compiled.Costs[0].Count, 3);
    EXPECT_EQ(compiled.Costs[1].Count, 2);
    EXPECT_EQ(compiled.Costs[1].Key, "TActor.pool.energy");
    EXPECT_EQ(compiled.Costs[1].Read, "BindActionEntity<TActor>(player).Read().Pool().Energy()");
    EXPECT_EQ(compiled.Costs[1].Write,
        "ProjectActionField(ProjectActionField(BindActionEntity<TActor>(player).Guard(), [](auto& value) { return value.Pool(); }), [](auto& value) { return value.Energy(); })");
}

TEST(ActionCompilerTest, RejectsInvalidOwnersAndSuspensionInSynchronousScopes) {
    Rejects("action A for Missing {}", "unknown action owner");
    Rejects("action A for THealth {}", "owner must be a GameEntity");
    Rejects("reaction A for TActor {}", "expected 'on'");
    Rejects("action A for TActor { choose THealth Target; }", "requires a from sequence");
    Rejects("class TBad { method void F() { choose TActor Target; } }", "synchronous scope");
    Rejects("action A for TActor { choose TActor Target; choose TActor Target; }", "duplicate local");
    Rejects("action A for TActor { return; choose TActor Target; }", "unreachable");
}

TEST(ActionCompilerTest, RejectsImpurePredicatesAndUnsupportedDiceContexts) {
    Rejects("action A for TActor { choose TActor Target where roll(1, 6) > 0; }", "effectful action expression");
    Rejects("action A for TActor { choose TActor Target where Target.Health.Heal(1); }", "pure expression");
    Rejects("action A for TActor { choose TActor Target where Target.Level; }", "expected 'bool'");
    Rejects("action A for TActor { let X = roll(true, 6); }", "expected 'int'");
    Rejects("action A for TActor { let X = roll(6); }", "count and a die size");
    Rejects("class TBad { query int F() { return roll(1, 6); } }", "effectful action expression");
    Rejects("action A for TActor { let X = TActor {}; }", "transactional entity allocation");
}

TEST(ActionCompilerTest, RejectsInvalidCostsBeforeEmittingCpp) {
    Rejects("action A for TActor { cost @self.Level = 1; }", "Resource field");
    Rejects("action A for TActor { cost @self.Missing = 1; }", "unknown cost field");
    Rejects("action A for TActor { cost @self = 1; }", "Resource field");
    Rejects("action A for TActor { cost @self.Pool.Energy() = 1; }", "stored field path");
    Rejects("action A for TActor { cost @self.Effort = -1; }", "cannot be negative");
    Rejects("action A for TActor { cost @self.Effort = @self.Level; }", "constant integer");
    Rejects("action A for TActor { cost @self.Effort = max_int + 1; }", "outside int range");
    Rejects("action A for TActor { cost @self.Effort = 1 / 0; }", "division by zero");
    Rejects("action A for TActor { cost @self.Effort = true; }", "expected 'int'");
    Rejects("action A for TActor { let X = 1; cost @self.Effort = 1; }", "must precede");
    Rejects("GameEntity TBad { derive Resource Effort = 1; } action A for TBad { cost @self.Effort = 1; }", "stored scalar fields");
}

TEST(ActionCompilerTest, SupportsOtherEntityTypesAndGuardedScalarWrites) {
    const auto loaded = Rules(R"(
        action Recharge for TActor {
            choose TDevice Target where Target.Charges < 5;
            Target.Charges += 1;
            @self.Level = 2;
            return;
        }
    )");
    const auto body = Body(CompileAction(loaded.Primary.Abilities.front(), loaded));
    EXPECT_NE(body.find("SelectActionEntity<::TDevice>"), std::string::npos);
    EXPECT_NE(body.find("ctx.AssignGuarded(ctx.Project(bound_Target.Guard()"), std::string::npos);
    EXPECT_NE(body.find("ctx.AssignGuarded(ctx.Project(action_self.Guard()"), std::string::npos);
    EXPECT_EQ(body.find("this"), std::string::npos);
}
