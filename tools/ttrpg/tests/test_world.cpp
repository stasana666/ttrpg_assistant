#include <gtest/gtest.h>

#include "compile_helpers.h"

#include <ttrpg/program.h>

#include <stdexcept>
#include <string>

namespace {

const std::string kTypes = R"(
    class THealth {
        int Current = 10;
        method void Heal(int Amount) { @self.Current += Amount; }
    }
    class TItem { int Reach = 2; }
    GameEntity TActor {
        Resource Effort = 3;
        THealth Health;
        int Level = 1;
        query bool IsAlive() { return @self.Health.Current > 0; }
    }
    GameEntity TObject { int Value; }
)";

TLoadedSchemas Rules(const std::string& source) {
    return LoadRules("world_rules", kTypes + source);
}

std::string Implementation(const TLoadedSchemas& loaded) {
    return EmitImplText(loaded, "world_rules");
}

void Rejects(const std::string& source, const std::string& diagnostic) {
    ExpectRejected(source, diagnostic, [&] { Implementation(Rules(source)); });
}

}

TEST(WorldCompilerTest, SeparatesPreconditionsAndLeadingChoiceFromActionEffects) {
    const auto loaded = Rules(R"(
        action Strike for TActor using TItem Item {
            requires @self.IsAlive();
            cost @self.Effort = 1;
            requires Item.Reach > 0;
            choose TActor Target where Target != @self &&
                grid_distance(@self, Target) <= Item.Reach && has_line_of_effect(@self, Target);
            let Amount = roll(1, 6);
            Target.Health.Heal(Amount);
            require Target.Level > 0;
        }
    )");
    const auto& declaration = loaded.Primary.Abilities.front();
    EXPECT_EQ(declaration.Body.front().Kind, TClassDecl::TStatement::EKind::Requires);
    const auto compiled = CompileAction(declaration, loaded);
    ASSERT_EQ(compiled.Costs.size(), 1u);
    EXPECT_EQ(compiled.Costs.front().Count, 1);
    const auto requirement = Join(compiled.Requirement);
    const auto body = Join(compiled.Body);
    EXPECT_NE(requirement.find("world.Bind<::TActor>(actor)"), std::string::npos);
    EXPECT_NE(requirement.find("FindAbilitySource<::TItem>"), std::string::npos);
    EXPECT_NE(requirement.find("world.Entities<::TActor>()"), std::string::npos);
    EXPECT_NE(requirement.find("source_value->Value"), std::string::npos);
    EXPECT_NE(body.find("source_Item.Read()"), std::string::npos);
    EXPECT_NE(body.find("require failed"), std::string::npos);
    EXPECT_EQ(requirement.find("RollActionDice"), std::string::npos);
    EXPECT_EQ(requirement.find("Heal"), std::string::npos);
    EXPECT_EQ(body.find(".requires"), std::string::npos);
    EXPECT_EQ(body.find("source_value"), std::string::npos);
    EXPECT_NE(Implementation(loaded).find("const TGeneratedWorldView& world"), std::string::npos);
}

TEST(WorldCompilerTest, DoesNotSpeculatePastExecutableStatements) {
    for (const auto& before : {std::string("let Reach = roll(1, 6);"), std::string("require @self.IsAlive();"),
                              std::string("@self.Health.Heal(1);")}) {
        const auto loaded = Rules("action A for TActor { cost @self.Effort = 1; " + before + " choose TActor Target; }");
        EXPECT_TRUE(CompileAction(loaded.Primary.Abilities.front(), loaded).Requirement.empty());
    }
    const auto loaded = Rules("action A for TActor { requires @self.Level > 0; let Reach = 2; choose TActor Target; }");
    const auto requirement = Join(CompileAction(loaded.Primary.Abilities.front(), loaded).Requirement);
    EXPECT_NE(requirement.find("Level()"), std::string::npos);
    EXPECT_NE(requirement.find("Entities"), std::string::npos);
    const auto leading = Rules("action A for TActor { choose TObject Target; }");
    EXPECT_NE(Join(CompileAction(leading.Primary.Abilities.front(), leading).Requirement).find("world.Entities<::TObject>()"), std::string::npos);
}

TEST(WorldCompilerTest, ReadsResourceBudgetsOnlyInPreflightWhenDeclaredAsRequirements) {
    const auto loaded = Rules(R"(
        action Budget for TActor {
            requires @self.Effort.Count() >= 3;
            cost @self.Effort = 1;
            choose TActor Target;
            Target.Health.Heal(1);
        }
    )");
    const auto compiled = CompileAction(loaded.Primary.Abilities.front(), loaded);
    EXPECT_NE(Join(compiled.Requirement).find(".Count() >= 3"), std::string::npos);
    EXPECT_EQ(Join(compiled.Body).find(".Count()"), std::string::npos);
    Rejects("action A for TActor { requires @self.Effort.Count(1) > 0; }", "Resource supports Count()");
    Rejects("action A for TActor { @self.Effort.Increase(1); }", "Resource supports Count()");
    Rejects("action A for TActor { let Count = @self.Level.Count(); }", "method receiver must be a class");
}

TEST(WorldCompilerTest, ExposesWorldQueriesToTypedFeatureHooks) {
    const auto loaded = Rules(R"(
        event int Bonus for TActor(TActor Target);
        feature Teamwork {
            on Bonus {
                let Count = 0;
                for Ally in entities(TActor) {
                    let Alias = Ally;
                    if (Alias != @self && Alias != Target && same_team(@self, Alias) &&
                        grid_distance(Alias, Target) <= 1 && has_line_of_effect(Alias, Target)) {
                        Count += position_x(Alias) - position_y(Alias);
                    }
                }
                return Count;
            }
        }
    )");
    const auto implementation = Implementation(loaded);
    EXPECT_NE(implementation.find("const TGeneratedWorldView& world"), std::string::npos);
    EXPECT_NE(implementation.find("world.Entities<TActor>()"), std::string::npos);
    EXPECT_NE(implementation.find("local_Alias.Read().Id()"), std::string::npos);
    EXPECT_NE(implementation.find("world.SameTeam(action_self.View(), local_Alias.View())"), std::string::npos);
    EXPECT_NE(implementation.find("world.PositionX(local_Alias.View())"), std::string::npos);
    EXPECT_NE(implementation.find("world.PositionY(local_Alias.View())"), std::string::npos);
    EXPECT_EQ(implementation.find("local_Alias.Guard()"), std::string::npos);
    EXPECT_EQ(implementation.find("auto local_Ally = *"), std::string::npos);
}

TEST(WorldCompilerTest, CapturesWorldAndReadonlyEntitySequencesAcrossChoices) {
    const auto loaded = Rules(R"(
        action Survey for TActor {
            let Others = entities(TActor);
            choose TActor Target where position_x(Target) >= position_x(@self);
            let Count = 0;
            for Ally in Others {
                let Alias = Ally;
                if (same_team(@self, Alias)) { Count += Alias.Level; }
            }
            @self.Health.Heal(Count);
            Target.Level += Count;
        }
    )");
    const auto body = Join(CompileAction(loaded.Primary.Abilities.front(), loaded).Body);
    EXPECT_NE(body.find("[Context, action_self, frame]"), std::string::npos);
    EXPECT_NE(body.find("std::optional<std::vector<TGeneratedEntityBinding<::TActor>>> slot_0"), std::string::npos);
    EXPECT_NE(body.find("auto& local_Others = frame.slot_0.value()"), std::string::npos);
    EXPECT_NE(body.find("ValidateActionEntity(Context, local_Alias.View())"), std::string::npos);
    EXPECT_NE(body.find("bound_Target.Guard()"), std::string::npos);
    EXPECT_NE(body.find("action_self.Guard()"), std::string::npos);
    EXPECT_EQ(body.find("local_Alias.Guard()"), std::string::npos);
}

TEST(WorldCompilerTest, RejectsMutationThroughReadonlyEntitiesAndAliases) {
    for (const auto& mutation : {std::string("Ally.Level = 2;"), std::string("Ally.Health.Heal(2);"),
                                 std::string("let Alias = Ally; Alias.Health.Current += 1;"),
                                 std::string("let Alias = Ally; Alias.Health.Heal(2);")}) {
        const auto expected = mutation.find(".Heal(") != std::string::npos ? "stable persistent receiver" : "stable owner";
        Rejects("action A for TActor { for Ally in entities(TActor) { " + mutation + " } }", expected);
        Rejects("action A for TActor { let Others = entities(TActor); choose TActor Target; for Ally in Others { " + mutation + " } }", expected);
    }
}

TEST(WorldCompilerTest, RejectsInvalidWorldQueryTypesAndArity) {
    Rejects("action A for TActor { let X = entities(); }", "one GameEntity type name");
    Rejects("action A for TActor { let X = entities(@self); }", "one GameEntity type name");
    Rejects("action A for TActor { let X = entities(TItem); }", "declared GameEntity type");
    Rejects("action A for TActor { let X = entities(Missing); }", "declared GameEntity type");
    Rejects("action A for TActor { let X = grid_distance(@self, 1); }", "checked GameEntity bindings");
    Rejects("action A for TActor { let X = same_team(@self, TItem {}); }", "checked GameEntity bindings");
    Rejects("action A for TActor { let X = has_line_of_effect(@self); }", "wrong argument count");
    Rejects("action A for TActor { let X = position_x(@self, @self); }", "wrong argument count");
    Rejects("action A for TActor { let X = position_y(2); }", "checked GameEntity bindings");
    Rejects("action A for TActor { requires grid_distance(@self, @self); }", "expected 'bool'");
}

TEST(WorldCompilerTest, RejectsWorldQueriesWithoutAnImplicitWorldScope) {
    Rejects("class TProfile { query int Read() { return position_x(@self); } }", "world queries require");
    Rejects("class TProfile { method int Read() { return position_y(@self); } }", "world queries require");
    Rejects("class TProfile { int Count = position_x(@self); }", "world queries require");
    Rejects("class TProfile { query int Count() { let Result = 0; for Ally in entities(TActor) { Result += 1; } return Result; } }", "world queries require");
    Rejects("class TProfile { query int Read() { return position_x(@self); } } action A for TActor { let X = TProfile {}.Read(); }", "world queries require");
}

TEST(WorldCompilerTest, RejectsImpureOrMisplacedPreconditions) {
    Rejects("action A for TActor { requires roll(1, 6) > 0; }", "effectful action expression");
    Rejects("action A for TActor { requires @self.Health.Heal(1); }", "pure expression");
    Rejects("action A for TActor { requires @self.Level; }", "expected 'bool'");
    Rejects("action A for TActor { let X = 1; requires X > 0; }", "initial action prefix");
    Rejects("action A for TActor { if (true) { requires true; } }", "initial action prefix");
    Rejects("class TProfile { query bool Read() { requires true; return true; } }", "initial action prefix");
    Rejects("event int Bonus for TActor(); action A for TActor { requires Bonus(@self); }", "requires an action context");
    Rejects("event int Bonus for TActor(); feature F { on Bonus { requires true; return 1; } }", "initial action prefix");
}

TEST(WorldCompilerTest, ReservesWorldBuiltinNamesForExpressions) {
    for (const auto& name : {"entities", "grid_distance", "same_team", "has_line_of_effect", "position_x", "position_y"}) {
        Rejects("event int " + std::string(name) + " for TActor();", "conflicts with existing symbol");
    }
}

TEST(WorldCompilerTest, PureLeadingLocalsExtendChoicePreflightWithoutSharingExecutionState) {
    const auto loaded = Rules(R"(
        class TMovementRules { query bool Allowed(int Distance) { return Distance == 1; } }
        action Step for TActor {
            cost @self.Effort = 1;
            let Actor = @self;
            let Origin = position(Actor);
            let Rules = TMovementRules {};
            let Destinations = adjacent_positions(Origin);
            choose Position Next from Destinations where can_occupy(Actor, Next)
                && Rules.Allowed(max(Next.X - Origin.X, Next.Y - Origin.Y));
            move(@self, Origin, Next);
        }
    )");
    const auto compiled = CompileAction(loaded.Primary.Abilities.front(), loaded);
    const auto requirement = Join(compiled.Requirement);
    const auto body = Join(compiled.Body);
    EXPECT_NE(requirement.find("auto local_Actor = action_self;"), std::string::npos);
    EXPECT_NE(requirement.find("world.Position(local_Actor.View())"), std::string::npos);
    EXPECT_NE(requirement.find("world.AdjacentPositions(local_Origin)"), std::string::npos);
    EXPECT_NE(requirement.find("candidate : local_Destinations"), std::string::npos);
    EXPECT_NE(requirement.find("return false;"), std::string::npos);
    EXPECT_EQ(requirement.find("MoveActionEntity("), std::string::npos);
    EXPECT_EQ(requirement.find(".Guard()"), std::string::npos);
    EXPECT_EQ(requirement.find("SelectAction"), std::string::npos);
    EXPECT_EQ(requirement.find("Context"), std::string::npos);
    EXPECT_NE(body.find("world.Position(local_Actor.View())"), std::string::npos);
    EXPECT_NE(body.find("frame.slot_1"), std::string::npos);
    EXPECT_NE(body.find("MoveActionEntity("), std::string::npos);
    EXPECT_EQ(compiled.Costs.size(), 1u);
}

TEST(WorldCompilerTest, PrefixStopsAtEffectsWindowsActivitiesAndEventDispatch) {
    const auto shared = R"(
        activity int Calculation for TActor() { return 2; }
        trigger int Opening(int Pending);
        event int Modifier for TActor();
        class TCounter { int Value; method int Increment() { @self.Value += 1; return @self.Value; } }
        GameEntity TCounterActor { TCounter Counter; }
    )";
    for (const auto& expression : {"roll(1, 6)", "Calculation()", "Opening(0)", "Modifier(@self)"}) {
        const auto loaded = Rules(std::string(shared) + "action A for TActor { requires @self.Level > 0; let Pure = 1 / @self.Level; let Value = " + expression + "; choose TActor Target; }");
        const auto requirement = Join(CompileAction(loaded.Primary.Abilities.front(), loaded).Requirement);
        EXPECT_NE(requirement.find("Level()"), std::string::npos);
        EXPECT_EQ(requirement.find("local_Pure"), std::string::npos);
        EXPECT_EQ(requirement.find("Entities"), std::string::npos);
        EXPECT_EQ(requirement.find("Context"), std::string::npos);
    }
    const auto effectful = Rules(std::string(shared) + "action A for TCounterActor { let Value = @self.Counter.Increment(); choose TActor Target; }");
    EXPECT_TRUE(CompileAction(effectful.Primary.Abilities.front(), effectful).Requirement.empty());
    const auto withoutChoice = Rules("action A for TActor { let Value = 1 / @self.Level; @self.Health.Heal(Value); }");
    EXPECT_TRUE(CompileAction(withoutChoice.Primary.Abilities.front(), withoutChoice).Requirement.empty());
}

TEST(WorldCompilerTest, PurePrefixPreservesSourceAndReactionParameterReads) {
    const auto loaded = Rules(R"(
        trigger int Opening(int Pending, TActor Subject);
        reaction Reply for TActor using TItem Item on Opening {
            cost @self.Effort = 1;
            let Other = Subject;
            let Reach = Item.Reach;
            let Amount = Pending;
            choose TActor Target where Target != Other && grid_distance(Other, Target) <= Reach;
            return Amount;
        }
    )");
    const auto compiled = CompileReaction(loaded.Primary.Abilities.front(), loaded);
    const auto requirement = Join(compiled.Execution.Requirement);
    EXPECT_NE(requirement.find("auto local_Other = std::get<0>(arguments)"), std::string::npos);
    EXPECT_NE(requirement.find("source_value->Value"), std::string::npos);
    EXPECT_NE(requirement.find("local_Reach"), std::string::npos);
    EXPECT_NE(requirement.find("local_Amount = pending"), std::string::npos);
    EXPECT_EQ(requirement.find(".Guard()"), std::string::npos);
    EXPECT_NO_THROW(Implementation(loaded));
}
