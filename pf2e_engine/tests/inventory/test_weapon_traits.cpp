#include <gtest/gtest.h>

#include <pf2e_engine/common/variant_map.h>
#include <pf2e_engine/inventory/dice.h>
#include <pf2e_engine/inventory/remaster_profiles.h>
#include <pf2e_engine/inventory/weapon.h>

#include <string>

namespace {

std::string Describe(const TWeaponTrait& t) {
    return std::visit(overloaded{
        [](const TWeaponTraitAgile&)       { return std::string("agile"); },
        [](const TWeaponTraitFinesse&)     { return std::string("finesse"); },
        [](const TWeaponTraitNonlethal&)   { return std::string("nonlethal"); },
        [](const TWeaponTraitFatal& x)     { return "fatal " + ToString(x.Die); },
        [](const TWeaponTraitDeadly& x)    { return "deadly " + ToString(x.Die); },
        [](const TWeaponTraitVersatile& x) { return "versatile " + ToString(x.Type); },
        [](const TWeaponTraitThrown& x)    { return "thrown " + std::to_string(x.RangeFeet); },
        [](const TWeaponTraitTwoHand& x)   { return "two-hand " + ToString(x.Die); },
    }, t.Payload());
}

}

TEST(WeaponTraitTest, AbsentTraitsAndAlternativesReadAsEmpty) {
    EXPECT_TRUE(TWeapon::Create({}).Traits()->empty());
    TVariantMap<EWeaponTraitKind, TWeaponTrait> traits;
    traits.Set(TWeaponTraitAgile{});
    EXPECT_EQ(traits.Get<TWeaponTraitFatal>(), nullptr);
    EXPECT_FALSE(traits.Has(EWeaponTraitKind::Fatal));
}

TEST(WeaponTraitTest, SameKindCollapsesToLastValue) {
    TVariantMap<EWeaponTraitKind, TWeaponTrait> traits;
    traits.Set(TWeaponTraitThrown{10});
    traits.Set(TWeaponTraitThrown{30});
    EXPECT_EQ(traits.size(), 1u);
    const auto* thrown = traits.Get<TWeaponTraitThrown>();
    ASSERT_NE(thrown, nullptr);
    EXPECT_EQ(thrown->RangeFeet, 30);
}

TEST(WeaponTraitTest, DescribeVisitsEveryAlternative) {
    EXPECT_EQ(Describe(TWeaponTraitAgile{}), "agile");
    EXPECT_EQ(Describe(TWeaponTraitNonlethal{}), "nonlethal");
    EXPECT_EQ(Describe(TWeaponTraitFatal{EDieSize::D8}), "fatal D8");
    EXPECT_EQ(Describe(TWeaponTraitVersatile{EDamageType::Piercing}), "versatile Piercing");
    EXPECT_EQ(Describe(TWeaponTraitThrown{20}), "thrown 20");
    EXPECT_EQ(Describe(TWeaponTraitTwoHand{EDieSize::D12}), "two-hand D12");
}

TEST(WeaponTraitTest, DaggerFromProductionData) {
    const TWeapon dagger = CreateDagger();

    EXPECT_TRUE(dagger.Traits().Has(EWeaponTraitKind::Agile));
    EXPECT_TRUE(dagger.Traits().Has(EWeaponTraitKind::Finesse));

    const auto* thrown = dagger.Traits().Get<TWeaponTraitThrown>();
    ASSERT_NE(thrown, nullptr);
    EXPECT_EQ(thrown->RangeFeet, 10);

    const auto* versatile = dagger.Traits().Get<TWeaponTraitVersatile>();
    ASSERT_NE(versatile, nullptr);
    EXPECT_EQ(versatile->Type, EDamageType::Slashing);
}
