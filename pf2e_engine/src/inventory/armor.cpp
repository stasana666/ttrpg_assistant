#include "armor.h"

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>

#include <stdexcept>

std::string ToString(EArmorCategory armor_category)
{
    switch (armor_category) {
        case EArmorCategory::Unarmored:
            return "Unarmored";
        case EArmorCategory::Light:
            return "Light";
        case EArmorCategory::Medium:
            return "Medium";
        case EArmorCategory::Heavy:
            return "Heavy";
    }
    throw std::runtime_error("incorrect value of EArmorCategory");
}

EArmorCategory ArmorCategoryFromString(std::string str_category)
{
    for (auto category : 
        { EArmorCategory::Unarmored
        , EArmorCategory::Light
        , EArmorCategory::Medium
        , EArmorCategory::Heavy})
    {
        if (ToString(category) == str_category) {
            return category;
        }
    }
    throw std::runtime_error("unknown EArmorCategory: \"" + str_category + "\"");
}

int TArmor::AcBonus() const
{
    return ac_bonus_;
}

int TArmor::DexCap() const
{
    return dex_cap_;
}

EArmorCategory TArmor::ArmorCategory() const
{
    return category_;
}

TAstNode TArmor::GetAst([[maybe_unused]] TAstContext& ctx) const
{
    static constexpr size_t kExpectedSize = 16;
    static constexpr size_t kExpectedSentinelOffset = 12;
    AST_ASSERT_LAYOUT_WITH_SENTINEL(TArmor, kExpectedSize, kExpectedSentinelOffset);
    AST_ASSERT_OFFSET(TArmor, category_, 0);
    AST_ASSERT_OFFSET(TArmor, ac_bonus_, 4);
    AST_ASSERT_OFFSET(TArmor, dex_cap_, 8);

    TAstNode node = TAstNode::MakeObject("TArmor");
    AddValueField(node, "category", category_);
    AddValueField(node, "ac_bonus", ac_bonus_);
    AddValueField(node, "dex_cap", dex_cap_);
    return node;
}
