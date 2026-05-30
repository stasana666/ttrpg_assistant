#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>

#include <limits>
#include <string>

enum class EArmorCategory {
    Unarmored,
    Light,
    Medium,
    Heavy,
};

std::string ToString(EArmorCategory armor_category);
EArmorCategory ArmorCategoryFromString(std::string armor_category);

class TArmor {
public:
    int AcBonus() const;
    int DexCap() const;
    EArmorCategory ArmorCategory() const;

    TAstNode GetAst(TAstContext& ctx) const;

private:
    friend class TGameObjectFactory;

    EArmorCategory category_{EArmorCategory::Unarmored};
    int ac_bonus_{0};
    int dex_cap_{std::numeric_limits<int>::max()};
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TArmor> : std::true_type {};
