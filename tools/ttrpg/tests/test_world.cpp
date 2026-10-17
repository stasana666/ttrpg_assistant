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

TEST(WorldCompilerTest, ParsesPreconditionsAndCostsBeforeTheLeadingChoice) {
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
}

TEST(WorldCompilerTest, DoesNotSpeculatePastExecutableStatements) {
    for (const auto& before : {std::string("let Reach = roll(1, 6);"), std::string("require @self.IsAlive();"),
                              std::string("@self.Health.Heal(1);")}) {
        const auto loaded = Rules("action A for TActor { cost @self.Effort = 1; " + before + " choose TActor Target; }");
        EXPECT_TRUE(CompileAction(loaded.Primary.Abilities.front(), loaded).Requirement.empty());
    }
}

TEST(WorldCompilerTest, ResourceReadsAreLimitedToCount) {
    Rejects("action A for TActor { requires @self.Effort.Count(1) > 0; }", "Resource supports Count()");
    Rejects("action A for TActor { @self.Effort.Increase(1); }", "Resource supports Count()");
    Rejects("action A for TActor { let Count = @self.Level.Count(); }", "method receiver must be a class");
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
    Rejects("action A for TActor { let X = map_positions(@self); }", "map_positions expects no arguments");
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

TEST(WorldCompilerTest, AcceptsPureLeadingLocalsBeforeAPositionChoice) {
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
    EXPECT_EQ(compiled.Costs.size(), 1u);
}

TEST(WorldCompilerTest, EffectsBeforeTheChoiceDisablePreflight) {
    const auto shared = R"(
        activity int Calculation for TActor() { return 2; }
        trigger int Opening(int Pending);
        event int Modifier for TActor();
        class TCounter { int Value; method int Increment() { @self.Value += 1; return @self.Value; } }
        GameEntity TCounterActor { TCounter Counter; }
    )";
    const auto effectful = Rules(std::string(shared) + "action A for TCounterActor { let Value = @self.Counter.Increment(); choose TActor Target; }");
    EXPECT_TRUE(CompileAction(effectful.Primary.Abilities.front(), effectful).Requirement.empty());
    const auto withoutChoice = Rules("action A for TActor { let Value = 1 / @self.Level; @self.Health.Heal(Value); }");
    EXPECT_TRUE(CompileAction(withoutChoice.Primary.Abilities.front(), withoutChoice).Requirement.empty());
}

TEST(WorldCompilerTest, AcceptsSourceAndReactionParameterReadsInThePrefix) {
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
    EXPECT_NO_THROW(Implementation(loaded));
}
