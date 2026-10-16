#include <gtest/gtest.h>

#include <ttrpg/activities.h>
#include <ttrpg/emit.h>
#include <ttrpg/parser.h>
#include <ttrpg/program.h>

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

TLoadedSchemas Rules(const std::string& source) {
    TLoadedSchemas loaded;
    loaded.PrimaryStem = "activities";
    loaded.Primary = TParser(Tokenize(R"(
        enum EOutcome { Yes, No }
        class TValue { int Amount = 1; }
        GameEntity TActor { int Level = 1; Resource Effort = 3; }
    )" + source)).Parse();
    RegisterModuleSymbols(loaded.PrimaryStem, loaded.Primary, loaded.SymbolTable);
    return loaded;
}

std::string Emit(const TLoadedSchemas& loaded) {
    std::ostringstream output;
    EmitImpl(output, loaded, "activities.ttrpg", "activities.h");
    return output.str();
}

void Rejects(const std::string& source, const std::string& diagnostic) {
    try {
        (void)Emit(Rules(source));
        FAIL() << "accepted invalid activity: " << source;
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find(diagnostic), std::string::npos) << error.what();
    }
}

}

TEST(ActivityCompilerTest, ParsesTypedDeclarationAndForwardCalls) {
    const auto loaded = Rules(R"(
        action Use for TActor { let Result = Child(@self, TValue {}, Yes); @self.Level = Result; }
        activity int Child for TActor(TActor Target, TValue Value, EOutcome Outcome) {
            choose int Extra from [1, 2];
            return Target.Level + Value.Amount + Extra;
        }
    )");
    const auto& declaration = loaded.Primary.Activities.front();
    EXPECT_EQ(declaration.Name, "Child");
    EXPECT_EQ(declaration.ReturnType, "int");
    ASSERT_EQ(declaration.Parameters.size(), 3u);
    const auto action = CompileAction(loaded.Primary.Abilities.front(), loaded);
    const auto& invocation = action.Flow.Blocks.front().Invocation;
    EXPECT_EQ(action.Flow.Blocks.front().Exit, TActionFlowBlock::EExit::Invoke);
    EXPECT_EQ(invocation.FunctionName, "::RunActivitiesChild");
    ASSERT_EQ(invocation.Arguments.size(), 3u);
    EXPECT_EQ(invocation.Arguments.front(), "action_self");
    ASSERT_TRUE(invocation.Slot);
    EXPECT_EQ(action.Flow.Slots[*invocation.Slot].CppType, "int");
    const auto activity = CompileActivity(declaration, loaded);
    EXPECT_EQ(activity.Parameters.front().CppType, "TGeneratedEntityBinding<::TActor>");
    EXPECT_EQ(activity.Flow.Initializers.size(), 3u);
    EXPECT_EQ(activity.Flow.EntryChecks.size(), 2u);
    EXPECT_NE(Emit(loaded).find("ValidateActionEntity(Context, arg_Target.View());"), std::string::npos);
}

TEST(ActivityCompilerTest, InvokesTypedChildrenAndCarriesResultsThroughBranchJoins) {
    const auto loaded = Rules(R"(
        activity int Child for TActor(int Base) { choose int Added from [1, 2]; return Base + Added; }
        activity int Parent for TActor(int Base) {
            let Total = Base;
            if (Base > 0) { let Value = Child(Base); Total += Value; }
            while (Total < 3) { let Added = Child(1); Total += Added; }
            return Total;
        }
    )");
    const auto parent = CompileActivity(loaded.Primary.Activities[1], loaded);
    EXPECT_EQ(std::count_if(parent.Flow.Blocks.begin(), parent.Flow.Blocks.end(), [](const auto& block) {
        return block.Exit == TActionFlowBlock::EExit::Invoke;
    }), 2);
    const auto output = Emit(loaded);
    EXPECT_NE(output.find("on_resume(TFlow::Run(Context, action_self, std::move(next)"), std::string::npos);
    EXPECT_NE(output.find("std::function<void(int)> on_resume"), std::string::npos);
    EXPECT_EQ(output.find("Then("), std::string::npos);
    EXPECT_EQ(output.find("shared_ptr<TFrame>"), std::string::npos);
}

TEST(ActivityCompilerTest, SupportsVoidEnumClassAndPositionResults) {
    const auto loaded = Rules(R"(
        activity void Nothing for TActor() { return; }
        activity EOutcome Decision for TActor() { return Yes; }
        activity TValue Report for TActor() { return TValue { Amount = 2; }; }
        activity Position Destination for TActor(Position Origin) { choose Position Next from adjacent_positions(Origin); return Next; }
        action Use for TActor {
            Nothing(); Decision();
            let Value = Report();
            let Next = Destination(Position { X = 1; Y = 2; });
            @self.Level = Value.Amount + Next.X;
        }
    )");
    for (const auto& declaration : loaded.Primary.Activities) EXPECT_NO_THROW(CompileActivity(declaration, loaded));
    const auto output = Emit(loaded);
    EXPECT_NE(output.find("TPosition RunActivitiesDestination"), std::string::npos);
    EXPECT_NE(output.find("std::function<void(TPosition)> on_resume"), std::string::npos);
    EXPECT_NE(output.find("if (!on_resume) throw std::invalid_argument"), std::string::npos);
}

TEST(ActivityCompilerTest, ValidatesReturnsAndPureArguments) {
    Rejects("activity int Missing for TActor() { if (true) { return 1; } }", "every path");
    Rejects("activity int Missing for TActor() { while (true) { return 1; } }", "every path");
    Rejects("activity int Missing for TActor() { for Value in entities(TActor) { return 1; } }", "every path");
    Rejects("activity int Missing for TActor() { return; }", "return value required");
    Rejects("activity int Wrong for TActor() { return true; }", "expected 'int'");
    Rejects("activity int F for TActor(int Value) { return Value; } action Use for TActor { F(roll(1, 6)); }", "roll");
    Rejects("activity void F for TActor() {} action Use for TActor { let Value = F(); }", "void activity result");
    Rejects("activity int F for TActor(int Value) { return Value; } action Use for TActor { F(); }", "argument count");
}

TEST(ActivityCompilerTest, RejectsMutableValueParametersAndEntityResults) {
    Rejects("activity void F for TActor(TValue Value) { Value.Amount = 3; }", "assignment");
    Rejects("activity void F for TActor(int Value) { Value = 3; }", "assign");
    Rejects("activity TActor F for TActor() { return @self; }", "activity result");
    Rejects("activity Resource F for TActor() { return @self.Effort; }", "activity result");
    Rejects("activity Feature F for TActor() {}", "activity result");
}

TEST(ActivityCompilerTest, RejectsCallsOutsideRootActionStatements) {
    const std::string function = "activity int F for TActor() { return 1; } ";
    Rejects(function + "action Use for TActor { let Value = F() + 1; }", "standalone");
    Rejects(function + "activity int G for TActor() { return F(); }", "standalone");
    Rejects(function + "action Use for TActor { for Target in entities(TActor) { F(); } }", "standalone");
    Rejects(function + "class TQuery { query int Value() { return F(); } }", "standalone");
    Rejects(function + "event int Bonus for TActor(); feature Helpful { on Bonus { return F(); } }", "standalone");
    Rejects("activity void F for TActor() { cost @self.Effort = 1; }", "cost");
    Rejects("activity void F for TActor() { requires true; }", "requires");
}

TEST(ActivityCompilerTest, RejectsRecursionAndConflictingSymbols) {
    Rejects("activity void F for TActor() { F(); }", "recursive activity");
    Rejects("activity void F for TActor() { G(); } activity void G for TActor() { F(); }", "recursive activity");
    Rejects("activity void TValue for TActor() {}", "conflicts");
    Rejects("activity void roll for TActor() {}", "conflicts");
    Rejects("class RunActivitiesF {} activity void F for TActor() {}", "conflicts with existing symbol");
    Rejects("activity void F for TValue() {}", "GameEntity");
    Rejects("activity void F for TActor(int Value, int Value) {}", "duplicate activity parameter");
    Rejects("GameEntity TOther {} activity void F for TOther() {} action Use for TActor { F(); }", "owner");
}

TEST(ActivityCompilerTest, ResolvesImportedActivitiesAndRejectsSiblingVisibility) {
    auto loaded = Rules("action Use for TActor { let Value = Imported(2); }");
    loaded.Primary.Imports.push_back("library.ttrpg");
    loaded.ByStem["library"] = TParser(Tokenize("activity int Imported for TActor(int Value) { return Value; }")).Parse();
    const auto output = Emit(loaded);
    EXPECT_NE(output.find("::RunLibraryImported(Context"), std::string::npos);
    loaded.Primary.Imports.clear();
    EXPECT_THROW(Emit(loaded), std::runtime_error);
}

TEST(ActivityCompilerTest, EmitsActivityOnlyModuleExportsAndRequiredIncludes) {
    const auto loaded = Rules("activity int F for TActor() { return 1; }");
    std::ostringstream header;
    EmitHeader(header, loaded, "activities.ttrpg", "activities.h");
    EXPECT_NE(header.str().find("int RunActivitiesF(std::shared_ptr<TActionContext> Context, std::function<void(int)> on_resume);"), std::string::npos);
    EXPECT_NE(Emit(loaded).find("#include <pf2e_engine/actions/generated_action.h>"), std::string::npos);
}

TEST(ActivityCompilerTest, CopiesSourceArgumentAndKeepsActorAnchorAcrossRuns) {
    const auto loaded = Rules(R"(
        activity int ReadValue for TActor(TValue Value) { choose int Extra from [1, 2]; return Value.Amount + Extra; }
        action Use for TActor using TValue Source { let Value = ReadValue(Source); @self.Level = Value; }
    )");
    const auto compiled = CompileAction(loaded.Primary.Abilities.front(), loaded);
    EXPECT_EQ(compiled.Flow.Blocks.front().Invocation.Arguments.front(), "source_Source.Read()");
    const auto output = Emit(loaded);
    EXPECT_NE(output.find("ValidateActionActorBinding(Context, action_self.View());"), std::string::npos);
    EXPECT_NE(output.find("[Context, action_self, frame, on_resume]"), std::string::npos);
}
