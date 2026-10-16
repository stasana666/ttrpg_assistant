#include <pf2e_engine/common/generated_execution.h>

#include <pf2e_engine/common/ast/ast_layout_assert.h>
#include <pf2e_engine/transformation/transformator.h>

#include <stdexcept>

TGeneratedAssignment::TGeneratedAssignment(std::unique_ptr<IValue> value)
    : value_(std::move(value))
{
}

void TGeneratedAssignment::Undo() noexcept
{
    value_->Swap();
}

void TGeneratedAssignment::Apply() noexcept
{
    value_->Swap();
}

TAstNode TGeneratedAssignment::GetAst(TAstContext& ctx) const
{
    static constexpr size_t kExpectedSize = 16;
    AST_ASSERT_LAYOUT(TGeneratedAssignment, kExpectedSize);
    return value_->GetAst(ctx);
}

TGeneratedExecutionContext::TGeneratedExecutionContext(TTransformator& transformator)
    : transformator_(&transformator)
{
}

void TGeneratedExecutionContext::ValidateField(const void* owner, std::string_view field)
{
    if (owner == nullptr || field.empty()) {
        throw std::invalid_argument("generated assignment requires an owner and field name");
    }
}

void TGeneratedExecutionContext::RecordAndApply(TGeneratedAssignment assignment)
{
    auto& record = transformator_->transformations_.emplace_back(std::move(assignment));
    std::get<TGeneratedAssignment>(record).Apply();
}

TState TGeneratedExecutionContext::GetState() const
{
    return transformator_->CurrentState();
}

void TGeneratedExecutionContext::Undo(TState state) const
{
    transformator_->Undo(state);
}

TGeneratedTransaction TGeneratedExecutionContext::Atomic() const
{
    return TGeneratedTransaction(*this);
}

TGeneratedTransaction::TGeneratedTransaction(TGeneratedExecutionContext context)
    : context_(context)
    , state_(context.GetState())
{
}

TGeneratedTransaction::~TGeneratedTransaction() noexcept
{
    if (!committed_) {
        context_.Undo(state_);
    }
}

void TGeneratedTransaction::Commit() noexcept
{
    committed_ = true;
}
