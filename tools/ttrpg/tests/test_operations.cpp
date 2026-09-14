#include <gtest/gtest.h>

#include <ttrpg/emit.h>
#include <ttrpg/parser.h>

#include <sstream>
#include <stdexcept>
#include <string>

namespace {

std::string Implementation(const std::string& source) {
    TLoadedSchemas loaded;
    loaded.PrimaryStem = "operations";
    loaded.Primary = TParser(Tokenize(R"(
        enum EDecision { Continue, Finish }
        variant TStatus { Prone; Frightened { int Value; } }
        class TItem { Resource Effort = 0; set<TStatus> Conditions; }
        GameEntity TActor {
            Resource Effort = 3;
            set<TStatus> Conditions;
            TItem Item;
            query bool IsAlive() { return true; }
        }
    )" + source)).Parse();
    RegisterModuleSymbols(loaded.PrimaryStem, loaded.Primary, loaded.SymbolTable);
    std::ostringstream output;
    EmitImpl(output, loaded, "operations.ttrpg", "operations.h");
    return output.str();
}

void Rejects(const std::string& source, const std::string& diagnostic) {
    try {
        Implementation(source);
        FAIL() << "expected rejection: " << source;
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find(diagnostic), std::string::npos) << error.what();
    }
}

}

TEST(OperationCompilerTest, EmitsJournaledStoredVariantAndResourceOperations) {
    const auto code = Implementation(R"(
        action A for TActor {
            @self.Conditions.set(Prone);
            @self.Conditions.set(Frightened { Value = 2; });
            @self.Conditions.erase(Prone);
            @self.Effort.Add(2);
            @self.Effort.Spend(1);
            @self.Item.Effort.SetCount(3);
        }
        class TRules {
            Resource Effort = 1;
            set<TStatus> Conditions;
            method void Reset() {
                @self.Conditions.erase(Frightened);
                @self.Effort.SetCount(3);
            }
        }
    )");
    for (const auto* operation : {"ctx.SetVariant(", "ctx.EraseVariant(", "ctx.AddResource(",
                                 "ctx.SpendResource(", "ctx.SetResourceCount("}) {
        EXPECT_NE(code.find(operation), std::string::npos);
    }
    EXPECT_NE(code.find("TStatusFrightened"), std::string::npos);
}

TEST(OperationCompilerTest, RejectsMutationsInPureContextsAndReadonlyProviders) {
    Rejects("class TBad { Resource Effort; query int Value() { @self.Effort.Add(1); return 0; } }", "pure");
    Rejects("action A for TActor { requires @self.Effort.Spend(1); }", "pure");
    Rejects("action A for TActor using TItem Item { Item.Conditions.set(Prone); }", "stable persistent field");
    Rejects("action A for TActor { for Ally in entities(TActor) { Ally.Effort.SetCount(1); } }", "stable persistent field");
    Rejects("event int Bonus for TActor(); feature F { on Bonus { @self.Conditions.erase(Prone); return 1; } }", "pure");
    Rejects("action A for TActor { @self.Conditions.erase(Missing); }", "unknown alternative");
    Rejects("action A for TActor { @self.Effort.Spend(true); }", "expected 'int'");
}

TEST(OperationCompilerTest, TypesPositionValuesChoicesAndExplicitMovementOrigins) {
    const auto code = Implementation(R"(
        action A for TActor {
            let Origin = position(@self);
            choose Position Next from adjacent_positions(Origin) where can_occupy(@self, Next);
            require contains_position(Next);
            require Next.X >= 0 && Next.Y >= 0;
            move(@self, Origin, Next);
            let Same = Next == Position { X = Next.X; Y = Next.Y; };
            require Same;
        }
    )");
    for (const auto* operation : {"SelectActionValue<TPosition>", "world.AdjacentPositions(",
                                 "world.CanOccupy(", "world.Contains(", "MoveActionEntity(Context,"}) {
        EXPECT_NE(code.find(operation), std::string::npos);
    }
    EXPECT_NE(code.find("TPosition{"), std::string::npos);
}

TEST(OperationCompilerTest, RejectsPositionTypeAndCapabilityErrors) {
    Rejects("action A for TActor { move(@self, 7); }", "expected 'Position'");
    Rejects("action A for TActor { for Ally in entities(TActor) { move(Ally, position(@self)); } }", "mutable");
    Rejects("action A for TActor { let P = position(@self); let X = P.Z; }", "only X and Y");
    Rejects("action A for TActor { let P = Position { X = 1; }; }", "both X and Y");
    Rejects("action A for TActor { let P = Position { X = true; Y = 1; }; }", "expected 'int'");
    Rejects("class TBad { Position Location; }", "Position");
    Rejects("class Position {}", "reserved builtin");
    Rejects("event int move for TActor();", "event name conflicts");
    Rejects("action A for TActor { choose int Value from [1, true]; }", "expected 'int'");
}

TEST(OperationCompilerTest, ReordersOnlyMutableEntitiesInExecutableBodies) {
    const auto code = Implementation(R"(
        trigger int Opening(int Pending, TActor Subject);
        activity void Reorder for TActor(TActor Target) {
            let Alias = Target;
            move_before_current_turn(Alias);
        }
        action A for TActor {
            move_before_current_turn(@self);
            choose TActor Target;
            Reorder(Target);
        }
        reaction Reply for TActor on Opening {
            move_before_current_turn(Subject);
            return Pending;
        }
    )");
    EXPECT_NE(code.find("pf2e_engine/actions/generated_initiative.h"), std::string::npos);
    EXPECT_NE(code.find("MoveActionEntityBeforeCurrentTurn(Context, local_Alias.View())"), std::string::npos);
    EXPECT_NE(code.find("MoveActionEntityBeforeCurrentTurn(Context, action_self.View())"), std::string::npos);
    EXPECT_NE(code.find("MoveActionEntityBeforeCurrentTurn(Context, arg_Subject.View())"), std::string::npos);
}

TEST(OperationCompilerTest, RejectsInitiativeMutationInPureScopesAndReadonlyAliases) {
    Rejects("action A for TActor { move_before_current_turn(); }", "expects one entity");
    Rejects("action A for TActor { move_before_current_turn(@self, @self); }", "expects one entity");
    Rejects("action A for TActor { move_before_current_turn(1); }", "checked GameEntity");
    Rejects("action A for TActor { move_before_current_turn(@self.Item); }", "checked GameEntity");
    Rejects("action A for TActor { for Other in entities(TActor) { let Alias = Other; move_before_current_turn(Alias); } }", "mutable entity");
    Rejects("action A for TActor { requires move_before_current_turn(@self); }", "effectful action");
    Rejects("class TBad { query int Read() { move_before_current_turn(@self); return 1; } }", "effectful action");
    Rejects("class TBad { method void Change() { move_before_current_turn(@self); } }", "effectful action");
    Rejects("event int Bonus for TActor(); feature F { on Bonus { move_before_current_turn(@self); return 1; } }", "effectful action");
    Rejects("TActor Example(int Value = move_before_current_turn(1)) {}", "effectful action");
    Rejects("activity void move_before_current_turn for TActor() {}", "conflicts");
    Rejects("event int move_before_current_turn for TActor();", "conflicts");
    Rejects("trigger int move_before_current_turn(int Pending);", "conflicts");
}
