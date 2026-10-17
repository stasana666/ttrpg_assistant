#include <gtest/gtest.h>

#include "compile_helpers.h"

#include <ttrpg/program.h>
#include <ttrpg/triggers.h>

#include <stdexcept>
#include <string>

namespace {

const std::string kOpening = "trigger TPending Opening(TPending Pending, TActor Subject, int Kind); ";

const std::string kTypes = R"(
    class TPending { int Amount = 0; bool Cancelled = false; }
    class TItem { string Name; int Amount = 3; }
    GameEntity TActor {
        int Value = 1;
        Resource Effort = 3;
        Resource Reactions = 1;
        collection<TItem> Items;
    }
)";

TLoadedSchemas Rules(const std::string& source) {
    return LoadRules("reactions", kTypes + source);
}

std::string Header(const TLoadedSchemas& loaded) {
    return EmitHeaderText(loaded, "reactions");
}

std::string Implementation(const TLoadedSchemas& loaded) {
    return EmitImplText(loaded, "reactions");
}


}

TEST(ReactionCompilerTest, ParsesExplicitStateAndMatchingSubscriptions) {
    const auto loaded = Rules(R"(
        trigger TPending Before(TPending Pending, TActor Subject, int Kind) once per activity;
        trigger TPending After(TPending Pending, TActor Subject, int Kind);
        reaction Counter for TActor using TItem Item on Before, After {
            cost @self.Reactions = 1;
            requires Subject != @self && Kind == 1;
            return TPending { Amount = Pending.Amount + Item.Amount; Cancelled = false; };
        }
    )");
    ASSERT_EQ(loaded.Primary.Triggers.size(), 2u);
    EXPECT_TRUE(loaded.Primary.Triggers[0].OncePerActivity);
    EXPECT_FALSE(loaded.Primary.Triggers[1].OncePerActivity);
    EXPECT_EQ(loaded.Primary.Abilities[0].Triggers, (std::vector<std::string>{"Before", "After"}));
    const auto compiled = CompileReaction(loaded.Primary.Abilities[0], loaded);
    EXPECT_EQ(compiled.StateType, "::TPending");
    EXPECT_EQ(compiled.ArgumentsType, "std::tuple<TGeneratedEntityBinding<::TActor>, int>");
    EXPECT_EQ(compiled.Execution.Costs.size(), 1u);
}

TEST(ReactionCompilerTest, WindowsUseSeparateFlowExitAndCopiedScope) {
    const auto loaded = Rules(kOpening + R"(
        activity int Repeat for TActor(TActor Subject) {
            let Total = 0;
            let Remaining = 2;
            while (Remaining > 0) {
                let Result = Opening(TPending { Amount = Total; }, Subject, 1);
                Total = Result.Amount;
                Remaining -= 1;
            }
            return Total;
        }
    )");
    const auto compiled = CompileActivity(loaded.Primary.Activities[0], loaded);
    EXPECT_TRUE(compiled.Flow.HasReactionScope);
    std::size_t windows = 0;
    for (const auto& block : compiled.Flow.Blocks) {
        if (block.Exit == TActionFlowBlock::EExit::Window) {
            ++windows;
            EXPECT_EQ(block.Window.Arguments.size(), 3u);
            EXPECT_TRUE(block.Window.Slot);
        }
    }
    EXPECT_EQ(windows, 1u);
}

TEST(ReactionCompilerTest, ReactionMaySuspendAndSelectIndependentSource) {
    const auto loaded = Rules(kOpening + R"(
        reaction Counter for TActor on Opening {
            cost @self.Reactions = 1;
            choose TItem Item from @self.Items where Item.Amount > Kind;
            choose int Extra from [1, 2];
            Subject.Value += Extra;
            return TPending { Amount = Pending.Amount + Item.Amount; Cancelled = false; };
        }
    )");
    const auto compiled = CompileReaction(loaded.Primary.Abilities[0], loaded);
    EXPECT_FALSE(compiled.Execution.Requirement.empty());
    EXPECT_TRUE(compiled.Execution.Flow.Activity);
    EXPECT_NO_THROW(Implementation(loaded));
}

TEST(ReactionCompilerTest, DiscardedWindowsStillUpdateScope) {
    const auto loaded = Rules("trigger int Opening(int Amount); action Act for TActor { Opening(1); Opening(2); }");
    const auto compiled = CompileAction(loaded.Primary.Abilities[0], loaded);
    std::size_t count = 0;
    for (const auto& block : compiled.Flow.Blocks) if (block.Exit == TActionFlowBlock::EExit::Window) {
        ++count;
        EXPECT_FALSE(block.Window.Slot);
    }
    EXPECT_EQ(count, 2u);
    EXPECT_NO_THROW(Implementation(loaded));
}

TEST(ReactionCompilerTest, PrimitiveEnumAndPositionStatesAreValues) {
    for (const auto& declaration : {
        "trigger int Opening(int Amount); reaction R for TActor on Opening { return Amount; }",
        "enum EResult { Allowed, Cancelled } trigger EResult Opening(EResult Value); reaction R for TActor on Opening { return Value; }",
        "trigger Position Opening(Position Value); reaction R for TActor on Opening { return Value; }"
    }) EXPECT_NO_THROW(Implementation(Rules(declaration)));
}

TEST(ReactionCompilerTest, RejectsIncompatibleDeclarationsAndSubscriptions) {
    for (const auto& declaration : {
        "trigger void Opening();",
        "trigger TActor Opening(TActor Subject);",
        "trigger int Opening(bool State);",
        "trigger int Opening();",
        "trigger int Opening(int State, int State);",
        "trigger int Opening(int State, Resource Cost);",
        "trigger int Opening(int State); reaction R for TItem on Opening { return State; }",
        "reaction R for TActor on Missing { return 1; }",
        "trigger int Opening(int State); reaction R for TActor on Opening, Opening { return State; }",
        "trigger int A(int State); trigger int B(int Other); reaction R for TActor on A,B { return State; }",
        "trigger int A(int State); trigger bool B(bool State); reaction R for TActor on A,B { return State; }",
        "trigger int A(int State, TItem Item); reaction R for TActor using TItem Item on A { return State; }"
    }) {
        SCOPED_TRACE(declaration);
        EXPECT_THROW(Header(Rules(declaration)), std::runtime_error);
    }
}

TEST(ReactionCompilerTest, RejectsWrongPayloadsEffectsAndUnscopedCalls) {
    for (const auto& source : {
        "action Act for TActor { let X = Opening(1, @self, 1); }",
        "action Act for TActor { let X = Opening(TPending {}, @self); }",
        "action Act for TActor { let X = Opening(TPending {}, @self, roll(1, 6)); }",
        "action Act for TActor { for Other in entities(TActor) { let X = Opening(TPending {}, Other, 1); } }",
        "action Act for TActor { let Targets = entities(TActor); for Other in Targets { Other.Value += 1; } }",
        "class TQuery { query int Value() { let X = Opening(TPending {}, TActor {}, 1); return 0; } }",
        "action Act for TActor { let X = Opening(TPending {}, @self, 1).Amount; }",
        "action Act for TActor { requires Opening(TPending {}, @self, 1).Cancelled; }",
        "reaction R for TActor on Opening { requires Subject.Value > 0; }",
        "reaction R for TActor on Opening { if (Kind > 0) { return Pending; } }",
        "reaction R for TActor on Opening { return 1; }"
    }) {
        SCOPED_TRACE(source);
        EXPECT_THROW(Header(Rules(kOpening + source)), std::runtime_error);
    }
}

TEST(ReactionCompilerTest, RejectsCyclesThroughSubscribersAndImportedActivities) {
    for (const auto& source : {
        "reaction R for TActor on Opening { let Next = Opening(Pending,Subject,Kind); return Next; }",
        "activity TPending Repeat for TActor(TPending State,TActor Actor,int Kind) { let Next=Opening(State,Actor,Kind); return Next; } reaction R for TActor on Opening { let Next=Repeat(Pending,Subject,Kind); return Next; }",
        "trigger TPending Other(TPending Pending,TActor Subject,int Kind); reaction A for TActor on Opening { let Next=Other(Pending,Subject,Kind);return Next;} reaction B for TActor on Other {requires false;let Next=Opening(Pending,Subject,Kind);return Next;}"
    }) {
        SCOPED_TRACE(source);
        try { Header(Rules(kOpening + source)); FAIL() << "cycle accepted"; }
        catch (const std::runtime_error& error) { EXPECT_NE(std::string(error.what()).find("recursive activity calls or reaction windows"), std::string::npos); }
    }
}

TEST(ReactionCompilerTest, RejectsTagAndRuntimeNameCollisions) {
    for (const auto& source : {
        "class TTriggerOpening {}",
        "class TReactionScope {}",
        "activity int Opening for TActor() { return 1; }",
        "feature Opening;",
        "class RunReactionsR {} reaction R for TActor on Opening { return Pending; }"
    }) EXPECT_THROW(Header(Rules(kOpening + source)), std::runtime_error);
}

TEST(ReactionCompilerTest, RejectsSelectedClassPayloadButAllowsPrimitiveSnapshots) {
    const auto source = R"(
        trigger TItem ItemOpening(TItem Pending);
        action Act for TActor { choose TItem Item from @self.Items; let Next = ItemOpening(Item); }
    )";
    EXPECT_THROW(Header(Rules(source)), std::runtime_error);
    EXPECT_NO_THROW(Header(Rules(R"(
        trigger TItem ItemOpening(TItem Pending);
        action Act for TActor { choose TItem Item from @self.Items; let Next = ItemOpening(TItem { Amount = Item.Amount; }); }
    )")));
}
