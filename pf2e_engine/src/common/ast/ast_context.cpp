#include <pf2e_engine/common/ast/ast_context.h>

#include <stdexcept>
#include <string>
#include <utility>

void TAstContext::Visit(const void* p, std::string_view type)
{
    if (p == nullptr) {
        return;
    }
    auto [_, inserted] = visited_.insert({p, std::string(type)});
    if (!inserted) {
        throw std::runtime_error(
            "AST ownership cycle detected through " + std::string(type));
    }
}

void TAstContext::Unvisit(const void* p, std::string_view type)
{
    if (p == nullptr) {
        return;
    }
    visited_.erase({p, std::string(type)});
}

void TAstContext::RegisterIdentity(const void* p, std::string id)
{
    if (p == nullptr) {
        return;
    }
    identity_[p] = std::move(id);
}

std::string TAstContext::IdentityOf(const void* p) const
{
    if (p == nullptr) {
        return "";
    }
    auto it = identity_.find(p);
    return it == identity_.end() ? "" : it->second;
}

TAstContext::TGuard::TGuard(TAstContext& ctx, const void* p, std::string_view type)
    : ctx_(ctx)
    , p_(p)
    , type_(type)
{
    ctx_.Visit(p_, type_);
}

TAstContext::TGuard::~TGuard()
{
    ctx_.Unvisit(p_, type_);
}
