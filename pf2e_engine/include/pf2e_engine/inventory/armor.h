#pragma once

#include <limits>
#include <string>

class TGameObjectFactory;

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

private:
    friend TGameObjectFactory;

    EArmorCategory category_{EArmorCategory::Unarmored};
    int ac_bonus_{0};
    int dex_cap_{std::numeric_limits<int>::max()};
};
