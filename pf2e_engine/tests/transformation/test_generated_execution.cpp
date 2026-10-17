#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/common/resource.h>
#include <pf2e_engine/transformation/transformator.h>

#include <mock_interaction_system.h>
#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace {

class TMutableValue {
public:
    int Value() const { return value_; }
    TGuarded<int> GuardedValue() { return TGuarded<int>(value_); }

    int Set(TGeneratedExecutionContext& ctx, int value)
    {
        auto transaction = ctx.Atomic();
        ctx.Assign(value_, value, this, "value");
        transaction.Commit();
        return value_;
    }

    void SetThenFail(TGeneratedExecutionContext& ctx, int value)
    {
        auto transaction = ctx.Atomic();
        ctx.Invoke(TGuarded<TMutableValue>(*this), &TMutableValue::Set, value);
        throw std::runtime_error("method failed");
    }

private:
    int value_ = 7;
};

TAstNode Snapshot(const TTransformator& transformator, const TResource& resource)
{
    TAstContext ctx;
    TAstNode node = TAstNode::MakeObject("execution");
    AddOwnedObject(node, "journal", transformator, ctx);
    ctx.RegisterIdentity(&resource, "resource");
    AddOwnedObject(node, "resource", resource, ctx);
    return node;
}

}

TEST(GeneratedExecutionTest, ScalarAndValueAssignmentsShareCheckpoint)
{
    TMockInteractionSystem io;
    TTransformator transformator(io);
    auto ctx = transformator.ExecutionContext();
    int count = 4;
    std::string name = "before";
    const auto initial = ctx.GetState();

    ctx.Assign(count, 9, &count, "count");
    ctx.Assign(name, "after", &name, "name");
    EXPECT_EQ(count, 9);
    EXPECT_EQ(name, "after");

    ctx.Undo(initial);
    EXPECT_EQ(count, 4);
    EXPECT_EQ(name, "before");
}

TEST(GeneratedExecutionTest, InvokeCallsGuardedMethodWithContext)
{
    TMockInteractionSystem io;
    TTransformator transformator(io);
    auto ctx = transformator.ExecutionContext();
    TMutableValue value;
    const auto initial = ctx.GetState();

    EXPECT_EQ(ctx.Invoke(TGuarded<TMutableValue>(value), &TMutableValue::Set, 19), 19);
    EXPECT_EQ(value.Value(), 19);

    ctx.Undo(initial);
    EXPECT_EQ(value.Value(), 7);
}

TEST(GeneratedExecutionTest, ProjectedNestedFieldWritesRemainJournaled)
{
    TMockInteractionSystem io;
    TTransformator transformator(io);
    auto ctx = transformator.ExecutionContext();
    TMutableValue value;
    const auto initial = ctx.GetState();
    const TGuarded<TMutableValue> owner(value);
    auto field = ctx.Project(owner, [](auto& object) { return object.GuardedValue(); });

    ctx.AssignGuarded(field, 19, owner, "value");
    EXPECT_EQ(value.Value(), 19);

    ctx.Undo(initial);
    EXPECT_EQ(value.Value(), 7);
}

TEST(GeneratedExecutionTest, OuterFailureUndoesCommittedNestedMethod)
{
    TMockInteractionSystem io;
    TTransformator transformator(io);
    auto ctx = transformator.ExecutionContext();
    TMutableValue value;
    TAstContext before_ctx;
    const auto before = transformator.GetAst(before_ctx);

    EXPECT_THROW(
        ctx.Invoke(TGuarded<TMutableValue>(value), &TMutableValue::SetThenFail, 19),
        std::runtime_error);

    EXPECT_EQ(value.Value(), 7);
    TAstContext after_ctx;
    EXPECT_EQ(transformator.GetAst(after_ctx), before);
}

TEST(GeneratedExecutionTest, FailedMethodPreservesEarlierCommittedChanges)
{
    TMockInteractionSystem io;
    TTransformator transformator(io);
    auto ctx = transformator.ExecutionContext();
    TMutableValue value;
    ctx.Invoke(TGuarded<TMutableValue>(value), &TMutableValue::Set, 12);
    TAstContext before_ctx;
    before_ctx.RegisterIdentity(&value, "value");
    const auto before = transformator.GetAst(before_ctx);

    EXPECT_THROW(
        ctx.Invoke(TGuarded<TMutableValue>(value), &TMutableValue::SetThenFail, 19),
        std::runtime_error);

    EXPECT_EQ(value.Value(), 12);
    TAstContext after_ctx;
    after_ctx.RegisterIdentity(&value, "value");
    EXPECT_EQ(transformator.GetAst(after_ctx), before);
}

TEST(GeneratedExecutionTest, UncommittedScopeRestoresState)
{
    TMockInteractionSystem io;
    TTransformator transformator(io);
    auto ctx = transformator.ExecutionContext();
    int value = 7;
    {
        auto transaction = ctx.Atomic();
        ctx.Assign(value, 12, &value, "value");
    }
    EXPECT_EQ(value, 7);
}

TEST(GeneratedExecutionTest, AssignmentAstContainsOwnerFieldAndPreviousValue)
{
    TMockInteractionSystem io;
    TTransformator transformator(io);
    auto ctx = transformator.ExecutionContext();
    TResource resource(3);

    ctx.Assign(resource, TResource(20), &resource, "value");

    const std::string ast = Snapshot(transformator, resource).PrettyPrint();
    EXPECT_NE(ast.find("TGeneratedAssignment"), std::string::npos);
    EXPECT_NE(ast.find("ref:resource"), std::string::npos);
    EXPECT_NE(ast.find("string:value"), std::string::npos);
    EXPECT_NE(ast.find("previous"), std::string::npos);
    EXPECT_NE(ast.find("int:3"), std::string::npos);
}

TEST(GeneratedExecutionTest, RejectsInvalidAssignmentMetadataWithoutMutation)
{
    TMockInteractionSystem io;
    TTransformator transformator(io);
    auto ctx = transformator.ExecutionContext();
    int value = 7;

    EXPECT_THROW(ctx.Assign(value, 9, nullptr, "value"), std::invalid_argument);
    EXPECT_THROW(ctx.Assign(value, 9, &value, ""), std::invalid_argument);
    EXPECT_EQ(value, 7);
}

TEST(GeneratedExecutionTest, RejectsForeignAndUnavailableCheckpoints)
{
    TMockInteractionSystem io;
    TTransformator first(io);
    TTransformator second(io);
    auto ctx = first.ExecutionContext();
    int value = 7;
    const auto initial = ctx.GetState();
    ctx.Assign(value, 9, &value, "value");
    const auto after_change = ctx.GetState();

    EXPECT_THROW(second.Undo(after_change), std::invalid_argument);
    EXPECT_EQ(value, 9);
    ctx.Undo(initial);
    EXPECT_THROW(ctx.Undo(after_change), std::invalid_argument);
    EXPECT_EQ(value, 7);
}
