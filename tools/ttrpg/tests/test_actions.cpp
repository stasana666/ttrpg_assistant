#include <gtest/gtest.h>

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
    TLoadedSchemas loaded;
    loaded.PrimaryStem = "compiled_actions";
    loaded.Primary = TParser(Tokenize(kTypes + source)).Parse();
    RegisterModuleSymbols(loaded.PrimaryStem, loaded.Primary, loaded.SymbolTable);
    return loaded;
}

std::string Implementation(const TLoadedSchemas& loaded) {
    std::ostringstream result;
    EmitImpl(result, loaded, "compiled_actions.ttrpg", "compiled_actions.h");
    return result.str();
}

std::string Body(const TCompiledAction& action) {
    std::string result;
    for (const auto& line : action.Body) result += line + "\n";
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
    const auto body = Body(compiled);
    EXPECT_NE(body.find("ActionSelf<::TActor>(Context)"), std::string::npos);
    EXPECT_NE(body.find("SelectActionEntity<::TActor>"), std::string::npos);
    EXPECT_NE(body.find("RollActionDice(Context, 1, 6)"), std::string::npos);
    EXPECT_NE(body.find("ctx.Invoke(ctx.Project(bound_Target.Guard()"), std::string::npos);
    EXPECT_NE(body.find("&::THealth::Heal, local_Amount"), std::string::npos);
    EXPECT_EQ(body.find("Health_"), std::string::npos);
    EXPECT_EQ(body.find("ParseDsl"), std::string::npos);
    EXPECT_EQ(body.find("transaction"), std::string::npos);
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
    const auto implementation = Implementation(loaded);
    EXPECT_NE(implementation.find("TAction::TBoundResourceCost{"), std::string::npos);
    EXPECT_EQ(implementation.find("EResourceKind"), std::string::npos);
}

TEST(ActionCompilerTest, CapturesValuesBeforeEachChoiceAndRecreatesContinuationLocals) {
    const auto loaded = Rules(R"(
        action Relay for TActor {
            let Amount = roll(1, 6);
            choose TActor First;
            Amount += 1;
            let Alias = First;
            choose TActor Second where Second != Alias;
            Amount += 2;
            if (Amount > 2) { Second.Health.Heal(Amount); }
            require @self.IsAlive();
        }
    )");
    const auto body = Body(CompileAction(loaded.Primary.Abilities.front(), loaded));
    EXPECT_NE(body.find("std::optional<int> slot_0"), std::string::npos);
    EXPECT_NE(body.find("auto& local_Amount = frame.slot_0.value()"), std::string::npos);
    EXPECT_NE(body.find("auto next = frame;"), std::string::npos);
    EXPECT_NE(body.find("frame.slot_2.emplace(bound_First)"), std::string::npos);
    EXPECT_NE(body.find("bound_Second.Read().Id() != local_Alias.Read().Id()"), std::string::npos);
    EXPECT_NE(body.find("ValidateActionActorBinding(Context, action_self.View());"), std::string::npos);
    EXPECT_NE(body.find("ValidateActionEntity(Context, bound_First.View());"), std::string::npos);
    EXPECT_EQ(body.find("mutable"), std::string::npos);
    EXPECT_EQ(body.find("[&"), std::string::npos);
    EXPECT_EQ(body.find("shared_ptr<Frame"), std::string::npos);
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

TEST(ActionCompilerTest, RegistersStableExecutorsAndImportedRules) {
    auto loaded = Rules("action Mend for TActor { @self.Health.Heal(1); }");
    loaded.Primary.Imports.push_back("base.ttrpg");
    loaded.ByStem["base"] = TParser(Tokenize("action External;")).Parse();
    const auto implementation = Implementation(loaded);
    EXPECT_NE(implementation.find("void RegisterCompiledActionsRules(TAbilityRules& rules)"), std::string::npos);
    EXPECT_NE(implementation.find("RegisterCompiledActionsAbilities(rules.Catalog);"), std::string::npos);
    EXPECT_NE(implementation.find("RegisterBaseRules(rules);"), std::string::npos);
    EXPECT_NE(implementation.find("static const auto action_Mend = std::make_shared<const TAction>"), std::string::npos);
    EXPECT_NE(implementation.find("TAction::TCompiledBody{"), std::string::npos);
    EXPECT_NE(implementation.find("rules.Executions.Register(\"compiled_actions.Mend\", action_Mend);"), std::string::npos);
    EXPECT_NE(implementation.find("#include <pf2e_engine/actions/generated_action.h>"), std::string::npos);
    std::ostringstream header;
    EmitHeader(header, loaded, "compiled_actions.ttrpg", "compiled_actions.h");
    EXPECT_NE(header.str().find("void RegisterCompiledActionsRules(TAbilityRules& rules);"), std::string::npos);
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
