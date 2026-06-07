#include <gtest/gtest.h>

#include <pf2e_engine/common/variant_map.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/inventory/dice.h>
#include <pf2e_engine/inventory/weapon.h>

#include <cpp_config.h>

#include <nlohmann/json.hpp>

#include <string>

namespace {

nlohmann::json WeaponJson(nlohmann::json traits) {
    return {
        {"name", "test_weapon"},
        {"base_dice_size", 6},
        {"damage_type", "Slashing"},
        {"category", "Martial"},
        {"traits", std::move(traits)},
    };
}

TWeapon ParseWeapon(nlohmann::json traits) {
    TGameObjectFactory factory;
    return TWeapon::FromJson(WeaponJson(std::move(traits)), factory);
}

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


TEST(WeaponTraitTest, FlagForm) {
    const TWeapon w = ParseWeapon({"Agile", "Finesse"});
    EXPECT_TRUE(w.Traits().Has(EWeaponTraitKind::Agile));
    EXPECT_TRUE(w.Traits().Has(EWeaponTraitKind::Finesse));
    EXPECT_FALSE(w.Traits().Has(EWeaponTraitKind::Nonlethal));
    EXPECT_EQ(w.Traits().size(), 2u);
}

TEST(WeaponTraitTest, SingleFieldFormEnumPayload) {
    const TWeapon w = ParseWeapon({{{"Fatal", "D10"}}});
    const auto* fatal = w.Traits().Get<TWeaponTraitFatal>();
    ASSERT_NE(fatal, nullptr);
    EXPECT_EQ(fatal->Die, EDieSize::D10);
}

TEST(WeaponTraitTest, SingleFieldFormIntPayload) {
    const TWeapon w = ParseWeapon({{{"Thrown", 20}}});
    const auto* thrown = w.Traits().Get<TWeaponTraitThrown>();
    ASSERT_NE(thrown, nullptr);
    EXPECT_EQ(thrown->RangeFeet, 20);
}

TEST(WeaponTraitTest, MixedFlagsAndParameterized) {
    const TWeapon w = ParseWeapon({"Finesse", {{"Fatal", "D8"}}, {{"Versatile", "Piercing"}}});
    EXPECT_TRUE(w.Traits().Has(EWeaponTraitKind::Finesse));

    const auto* fatal = w.Traits().Get<TWeaponTraitFatal>();
    ASSERT_NE(fatal, nullptr);
    EXPECT_EQ(fatal->Die, EDieSize::D8);

    const auto* versatile = w.Traits().Get<TWeaponTraitVersatile>();
    ASSERT_NE(versatile, nullptr);
    EXPECT_EQ(versatile->Type, EDamageType::Piercing);
}

TEST(WeaponTraitTest, GetReturnsNullForAbsentAlternative) {
    const TWeapon w = ParseWeapon({"Agile"});
    EXPECT_EQ(w.Traits().Get<TWeaponTraitFatal>(), nullptr);
    EXPECT_FALSE(w.Traits().Has(EWeaponTraitKind::Fatal));
}

TEST(WeaponTraitTest, EmptyTraitsWhenAbsent) {
    TGameObjectFactory factory;
    nlohmann::json j = {
        {"name", "plain"},
        {"base_dice_size", 6},
        {"damage_type", "Slashing"},
        {"category", "Martial"},
    };
    const TWeapon w = TWeapon::FromJson(j, factory);
    EXPECT_TRUE(w.Traits().empty());
}


TEST(WeaponTraitTest, FlagGivenWithParametersThrows) {
    EXPECT_THROW(ParseWeapon({{{"Agile", 5}}}), std::runtime_error);
}

TEST(WeaponTraitTest, ParameterizedGivenWithoutParametersThrows) {
    EXPECT_THROW(ParseWeapon({"Fatal"}), std::runtime_error);
}

TEST(WeaponTraitTest, MultiKeyObjectThrows) {
    nlohmann::json bad = nlohmann::json::array();
    bad.push_back({{"Fatal", "D8"}, {"Thrown", 20}});
    EXPECT_THROW(ParseWeapon(bad), std::runtime_error);
}

TEST(WeaponTraitTest, UnknownTraitThrows) {
    EXPECT_THROW(ParseWeapon({"Whirling"}), std::runtime_error);
}


TEST(WeaponTraitTest, SameKindCollapsesToLastValue) {
    const TWeapon w = ParseWeapon({{{"Thrown", 10}}, {{"Thrown", 30}}});
    EXPECT_EQ(w.Traits().size(), 1u);
    const auto* thrown = w.Traits().Get<TWeaponTraitThrown>();
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
    TGameObjectFactory factory;
    factory.AddSource(kRootDirPath + "/pf2e_engine/data/inventory/weapon/dagger.json");
    const TWeapon dagger = factory.Create<TWeapon>(
        TGameObjectIdManager::Instance().Register("dagger"));

    EXPECT_TRUE(dagger.Traits().Has(EWeaponTraitKind::Agile));
    EXPECT_TRUE(dagger.Traits().Has(EWeaponTraitKind::Finesse));

    const auto* thrown = dagger.Traits().Get<TWeaponTraitThrown>();
    ASSERT_NE(thrown, nullptr);
    EXPECT_EQ(thrown->RangeFeet, 10);

    const auto* versatile = dagger.Traits().Get<TWeaponTraitVersatile>();
    ASSERT_NE(versatile, nullptr);
    EXPECT_EQ(versatile->Type, EDamageType::Slashing);
}
