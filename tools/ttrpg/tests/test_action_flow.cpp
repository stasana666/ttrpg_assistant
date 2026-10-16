#include <gtest/gtest.h>

#include "compile_helpers.h"

#include <ttrpg/program.h>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace {

TLoadedSchemas Rules(const std::string& source) {
    return LoadRules("flow", R"(
        enum EDecision { Continue, Finish }
        GameEntity TActor { int Level = 1; Resource Effort = 3; }
    )" + source);
}

TCompiledAction Compile(const std::string& source) {
    const auto loaded = Rules(source);
    return CompileAction(loaded.Primary.Abilities.front(), loaded);
}

void Rejects(const std::string& source, const std::string& diagnostic) {
    ExpectRejected(source, diagnostic, [&] { (void)EmitImplText(Rules(source), "flow"); });
}

}

TEST(ActionFlowCompilerTest, ParsesNestedChoiceSourcesAndWhileConditions) {
    const auto loaded = Rules(R"(
        action Loop for TActor {
            while (@self.Level > 0) {
                choose EDecision Decision from [Continue, Finish] where Decision != Finish;
                if (Decision == Continue) { choose int Amount from [1, 2]; }
            }
        }
    )");
    const auto& loop = loaded.Primary.Abilities.front().Body.front();
    EXPECT_EQ(loop.Kind, TClassDecl::TStatement::EKind::While);
    ASSERT_EQ(loop.Body.size(), 2u);
    ASSERT_TRUE(loop.Body[0].Candidates);
    ASSERT_TRUE(loop.Body[0].Value);
    EXPECT_EQ(loop.Body[0].Candidates->Args.size(), 2u);
    EXPECT_EQ(loop.Body[1].Body[0].TypeName, "int");
}

TEST(ActionFlowCompilerTest, BranchesJoinUsingOuterSlotsAndReleaseBranchBindings) {
    const auto compiled = Compile(R"(
        action Branch for TActor {
            let Total = 1;
            if (@self.Level > 0) {
                choose TActor Target;
                Total += Target.Level;
            } else {
                choose int Amount from [2, 3];
                Total += Amount;
            }
            @self.Level = Total;
        }
    )");
    const auto& flow = compiled.Flow;
    ASSERT_EQ(flow.Slots.size(), 3u);
    EXPECT_EQ(flow.Slots[0].CppType, "int");
    EXPECT_EQ(flow.Slots[1].CppType, "TGeneratedEntityBinding<::TActor>");
    const TActionFlowBlock* finalWrite = nullptr;
    std::size_t choices = 0;
    std::size_t cleaned = 0;
    for (const auto& block : flow.Blocks) {
        if (block.Exit == TActionFlowBlock::EExit::Choose) ++choices;
        const auto body = Join(block.Body);
        if (body.find("frame.slot_1.reset()") != std::string::npos || body.find("frame.slot_2.reset()") != std::string::npos) ++cleaned;
        if (body.find("ctx.AssignGuarded") != std::string::npos) finalWrite = &block;
    }
    EXPECT_EQ(choices, 2u);
    EXPECT_EQ(cleaned, 2u);
    ASSERT_NE(finalWrite, nullptr);
    EXPECT_EQ(finalWrite->LiveSlots, (std::vector<std::size_t>{0}));
    EXPECT_TRUE(compiled.Requirement.empty());
}

TEST(ActionFlowCompilerTest, LoopBackedgeCarriesOuterLocalsAndDropsIterationScope) {
    const auto compiled = Compile(R"(
        action Loop for TActor {
            let Remaining = 3;
            while (Remaining > 0) {
                let Step = Remaining;
                choose EDecision Decision from [Continue, Finish];
                if (Decision == Finish) { return; }
                choose TActor Target;
                Target.Level += Step;
                Remaining -= 1;
            }
            @self.Level = Remaining;
        }
    )");
    const auto& flow = compiled.Flow;
    ASSERT_EQ(flow.Slots.size(), 4u);
    std::size_t header = flow.Blocks.size();
    for (std::size_t index = 0; index < flow.Blocks.size(); ++index) {
        if (flow.Blocks[index].Condition.find("local_Remaining") != std::string::npos) header = index;
    }
    ASSERT_LT(header, flow.Blocks.size());
    EXPECT_EQ(flow.Blocks[header].LiveSlots, (std::vector<std::size_t>{0}));
    const TActionFlowBlock* backedge = nullptr;
    for (const auto& block : flow.Blocks) {
        if (block.Exit == TActionFlowBlock::EExit::Jump && block.Next == header) backedge = &block;
        for (const auto slot : block.LiveSlots) EXPECT_LT(slot, flow.Slots.size());
        if (block.Exit != TActionFlowBlock::EExit::Return) { EXPECT_LT(block.Next, flow.Blocks.size()); }
        if (block.Exit == TActionFlowBlock::EExit::Branch) { EXPECT_LT(block.Otherwise, flow.Blocks.size()); }
    }
    ASSERT_NE(backedge, nullptr);
    const auto cleanup = Join(backedge->Body);
    EXPECT_NE(cleanup.find("frame.slot_1.reset()"), std::string::npos);
    EXPECT_NE(cleanup.find("frame.slot_2.reset()"), std::string::npos);
    EXPECT_NE(cleanup.find("frame.slot_3.reset()"), std::string::npos);
    EXPECT_EQ(cleanup.find("frame.slot_0.reset()"), std::string::npos);
    const auto body = Join(compiled.Body);
    EXPECT_NE(body.find("while (true)"), std::string::npos);
    EXPECT_NE(body.find("switch (pc)"), std::string::npos);
    EXPECT_NE(body.find("auto next = frame;"), std::string::npos);
    EXPECT_EQ(body.find("mutable"), std::string::npos);
    EXPECT_EQ(body.find("[&"), std::string::npos);
    EXPECT_EQ(body.find("shared_ptr<TFrame>"), std::string::npos);
}

TEST(ActionFlowCompilerTest, AllowsReusedNamesInDisjointScopesAndKeepsSlotsDistinct) {
    const auto compiled = Compile(R"(
        action Scoped for TActor {
            if (true) { choose int Amount from [1]; } else { choose int Amount from [2]; }
            choose int Amount from [3];
            @self.Level = Amount;
        }
    )");
    ASSERT_EQ(compiled.Flow.Slots.size(), 3u);
    EXPECT_NE(compiled.Flow.Slots[0].Local, compiled.Flow.Slots[1].Local);
    EXPECT_NE(compiled.Flow.Slots[1].Local, compiled.Flow.Slots[2].Local);
}

TEST(ActionFlowCompilerTest, GeneratedSlotNamesCannotCollideWithAuthoredSuffixes) {
    const auto compiled = Compile(R"(
        action Names for TActor {
            if (true) { let A = 1; }
            if (true) { let A_2 = 2; }
            if (true) { let A = 3; @self.Level = A; }
        }
    )");
    ASSERT_EQ(compiled.Flow.Slots.size(), 3u);
    EXPECT_NE(compiled.Flow.Slots[1].Local, compiled.Flow.Slots[2].Local);
    const TActionFlowBlock* write = nullptr;
    for (const auto& block : compiled.Flow.Blocks) {
        if (Join(block.Body).find("ctx.AssignGuarded") != std::string::npos) write = &block;
    }
    ASSERT_NE(write, nullptr);
    EXPECT_EQ(write->LiveSlots, (std::vector<std::size_t>{2}));
}

TEST(ActionFlowCompilerTest, SynchronousLoopLocalsCannotShadowRenamedFrameAliases) {
    const auto compiled = Compile(R"(
        action Names for TActor {
            if (true) { let A = 1; }
            let A = 7;
            for A_1 in entities(TActor) {
                let A_1_1 = A_1;
                @self.Level = A;
            }
        }
    )");
    ASSERT_EQ(compiled.Flow.Slots.size(), 2u);
    const auto scalar = compiled.Flow.Slots[1].Local;
    const auto body = Join(compiled.Body);
    EXPECT_EQ(body.find("auto " + scalar + " = event_value_"), std::string::npos);
    EXPECT_EQ(body.find("auto " + scalar + " = local_"), std::string::npos);
    EXPECT_NE(body.find("auto& " + scalar + " = frame.slot_1.value()"), std::string::npos);
    EXPECT_NE(body.find(", " + scalar + ", action_self.Guard()"), std::string::npos);
}

TEST(ActionFlowCompilerTest, QualifiesTypesThatConflictWithDispatcherNamesAndVariables) {
    const auto compiled = Compile(R"(
        class TFlow { int Value = 1; }
        class TFrame { int Value = 2; }
        class Run { int Value = 3; }
        class Context { int Value = 4; }
        class frame { int Value = 5; }
        class pc { int Value = 6; }
        class slot_0 { int Value = 7; }
        action Names for TActor {
            let First = TFlow {};
            let Second = TFrame {};
            let Third = Run {};
            let Fourth = Context {};
            let Fifth = frame {};
            let Sixth = pc {};
            let Seventh = slot_0 {};
            choose int Amount from [1];
            @self.Level = First.Value + Second.Value + Third.Value + Fourth.Value + Fifth.Value + Sixth.Value + Seventh.Value + Amount;
        }
    )");
    EXPECT_NE(compiled.Flow.FlowType, "TFlow");
    EXPECT_NE(compiled.Flow.FrameType, "TFrame");
    EXPECT_NE(compiled.Flow.RunName, "Run");
    ASSERT_EQ(compiled.Flow.Slots.size(), 8u);
    const auto body = Join(compiled.Body);
    for (std::size_t index = 0; index != 7; ++index) {
        EXPECT_TRUE(compiled.Flow.Slots[index].CppType.starts_with("::"));
        EXPECT_NE(body.find(compiled.Flow.Slots[index].CppType + "::Create"), std::string::npos);
        EXPECT_NE(body.find("static_cast<const " + compiled.Flow.Slots[index].CppType + "&>"), std::string::npos);
    }
}

TEST(ActionFlowCompilerTest, PreflightUsesOnlyTheLeadingPureChoice) {
    const auto leading = Compile(R"(
        action Leading for TActor {
            requires @self.Level > 0;
            cost @self.Effort = 1;
            choose int Amount from [1, 2] where Amount > @self.Level;
            @self.Level = Amount;
        }
    )");
    const auto requirement = Join(leading.Requirement);
    EXPECT_NE(requirement.find("std::vector<int>{1, 2}"), std::string::npos);
    EXPECT_NE(requirement.find("candidate > (action_self.Read()).Level()"), std::string::npos);
    EXPECT_EQ(requirement.find("SelectAction"), std::string::npos);
    EXPECT_EQ(requirement.find("RollActionDice"), std::string::npos);
    const auto delayed = Compile(R"(
        action Delayed for TActor {
            cost @self.Effort = 1;
            let Count = roll(1, 6);
            while (Count > 0) { choose int Amount from [1]; Count -= Amount; }
        }
    )");
    EXPECT_TRUE(delayed.Requirement.empty());
}

TEST(ActionFlowCompilerTest, ChoosesPrimitiveEnumPositionAndReadonlyEntityCandidates) {
    const auto compiled = Compile(R"(
        action Values for TActor {
            choose string Label from ["one", "two"];
            choose bool Enabled from [true, false];
            choose Position Next from [Position { X = 1; Y = 2; }];
            let Actors = entities(TActor);
            choose TActor Target from Actors where Target != @self;
            Target.Level += Next.X;
        }
    )");
    ASSERT_EQ(compiled.Flow.Slots.size(), 5u);
    EXPECT_EQ(compiled.Flow.Slots[0].CppType, "std::string");
    EXPECT_EQ(compiled.Flow.Slots[1].CppType, "bool");
    EXPECT_EQ(compiled.Flow.Slots[2].CppType, "TPosition");
    EXPECT_EQ(compiled.Flow.Slots[3].CppType, "std::vector<TGeneratedEntityBinding<::TActor>>");
    const auto body = Join(compiled.Body);
    EXPECT_NE(body.find("SelectActionValue<TPosition>"), std::string::npos);
    EXPECT_NE(body.find("SelectActionEntity<::TActor>(Context, local_Actors"), std::string::npos);
    EXPECT_NE(body.find("bound_Target.Guard()"), std::string::npos);
}

TEST(ActionFlowCompilerTest, RejectsInvalidTypesImpurityEscapedLocalsAndUnreachableStatements) {
    Rejects("action A for TActor { choose int Value; }", "from sequence");
    Rejects("action A for TActor { choose int Value from [true]; }", "expected 'int'");
    Rejects("action A for TActor { choose int Value from 1; }", "sequence");
    Rejects("action A for TActor { choose int Value from [roll(1, 6)]; }", "effectful action expression");
    Rejects("action A for TActor { while (1) {} }", "expected 'bool'");
    Rejects("action A for TActor { while (roll(1, 6) > 0) {} }", "effectful action expression");
    Rejects("action A for TActor { if (true) { choose int Value from [1]; } @self.Level = Value; }", "unknown");
    Rejects("action A for TActor { while (false) { let Value = 1; } @self.Level = Value; }", "unknown");
    Rejects("action A for TActor { if (true) { return; } else { return; } choose int Value from [1]; }", "unreachable");
    Rejects("action A for TActor { while (true) { return; let Value = 1; } }", "unreachable");
    Rejects("action A for TActor { while (true) { cost @self.Effort = 1; } }", "must precede");
    Rejects("class TBad { query int Read() { while (true) {} return 1; } }", "synchronous scope");
    Rejects("action A for TActor { for Other in entities(TActor) { choose int Value from [1]; } }", "synchronous scope");
    Rejects("event int Bonus for TActor(); action A for TActor { let Values = Bonus(@self); Values = [1]; }", "matching sequence kinds");
}
