#include <gtest/gtest.h>

#include <pf2e_engine/resources.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/weapon_slot.h>
#include <pf2e_engine/weapon_damage_factory.h>

#include <mock_dice_roller.h>
#include <game_context_from_part.h>

TEST(WeaponDamageTest, LongSwordNormalHit) {
    auto hand = TResourceManager::instance().Register("hand");
    TResourcePool pool;
    pool.Add(hand, 2);

    TWeapon weapon(6, TDamage::Type::Slashing);
    
    TWeaponSlot slot(pool);
    slot.Equip(&weapon);

    // TODO: hands not supported yet
    // EXPECT_EQ(pool.Count(hand), 1);

    TCharacteristic str(18);

    TWeaponDamageFactory factory(slot, str);

    auto damage = factory.HitDamage();
    int count = 0;
    for (const auto& [type, expr] : damage) {
        ++count;
        EXPECT_EQ(type, TDamage::Type::Slashing);

        TMockRng rng;
        rng.ExpectCall(6, 4);
        auto ctx = GameContextFrom(rng);
        EXPECT_EQ(expr->Value(ctx), 8);
        rng.Verify();
    }
    EXPECT_EQ(count, 1);
}

TEST(WeaponDamageTest, LongSwordCritHit) {
    auto hand = TResourceManager::instance().Register("hand");
    TResourcePool pool;
    pool.Add(hand, 2);

    TWeapon weapon(6, TDamage::Type::Slashing);
    
    TWeaponSlot slot(pool);
    slot.Equip(&weapon);

    // TODO: hands not supported yet
    // EXPECT_EQ(pool.Count(hand), 1);

    TCharacteristic str(18);

    TWeaponDamageFactory factory(slot, str);

    auto crit_damage = factory.CritDamage();
    int count = 0;
    for (const auto& [type, expr] : crit_damage) {
        ++count;
        EXPECT_EQ(type, TDamage::Type::Slashing);

        TMockRng rng;
        rng.ExpectCall(6, 4);
        auto ctx = GameContextFrom(rng);
        EXPECT_EQ(expr->Value(ctx), 16);
        rng.Verify();
    }
    EXPECT_EQ(count, 1);
}
