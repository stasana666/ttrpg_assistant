#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>

class THitPoints {
public:
    explicit THitPoints(int max_hp);

    int GetCurrentHp() const;
    void ReduceHp(int damage);
    void RestoreHp(int heal);
    void SetTemporaryHp(int hp);
    int GetTemporaryHp() const;

    TAstNode GetAst(TAstContext& ctx) const;

private:
    int max_hp_;
    int current_hp_;
    int temporary_hp_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<THitPoints> : std::true_type {};
