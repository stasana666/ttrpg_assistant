#include <pf2e_engine/inventory/remaster_profiles.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

class TGeneratedSpellsTest : public TGeneratedCombatFixture {
protected:
    void SetUp() override
    {
        TGeneratedCombatFixture::SetUp();
        Join(CreateElfWizard5(), 0, {0, 0}, "Wizard");
        BeginTurn();
    }

    TStep Fireball(TPosition center, std::map<std::string, TField> changes)
    {
        changes.emplace("Wizard.actions.count", 1);
        changes.emplace("Wizard.spell_slots.third.count", 2);
        return {.Choose = {TPick::Value(ESpellSlotRank::Rank3), TPick::Value(center)},
                .Rolls = std::vector<std::pair<int, int>>(6, {6, 3}), .Changes = std::move(changes)};
    }
};

TEST_F(TGeneratedSpellsTest, FireballSpendsASlotAndDamagesEveryLivingCreatureInTheBurst)
{
    Join(CreateWolf({.Name = "Center"}), 1, {5, 5}, "Center");
    Join(CreateWolf({.Name = "Edge"}), 1, {9, 5}, "Edge");
    Join(CreateWolf({.Name = "Outside"}), 1, {10, 5}, "Outside");
    Join(CreateHumanFighter1({.Name = "Ally"}), 0, {5, 8}, "Ally");
    Join(CreateWolf({.Name = "Dead"}), 1, {4, 5}, "Dead");
    journal_->DealDamage(&Player(5), 24);
    Act("spells.CastFireball", 0, Fireball({5, 5}, {{"Center.hitpoints.current", 6}, {"Edge.hitpoints.current", 6},
                                                  {"Ally.hitpoints.current", 3}}));
}

TEST_F(TGeneratedSpellsTest, FireballNeedsAThirdRankOrHigherSlot)
{
    Join(CreateWolf(), 1, {5, 5}, "Wolf");
    EXPECT_TRUE(Available("spells.CastFireball"));
    auto execution = journal_->ExecutionContext();
    auto slots = Creature(0).SpellSlots();
    execution.SetResourceCount(execution.Project(slots, [](TSpellSlots& value) { return value.Third(); }), 0, slots,
                               "third");
    EXPECT_FALSE(Available("spells.CastFireball"));
}

TEST_F(TGeneratedSpellsTest, RemoveFearClearsFrightenedFromALivingCreatureWithinSixSquares)
{
    Join(CreateHumanFighter1({.Name = "Ally"}), 0, {6, 0}, "Ally");
    Join(CreateHumanFighter1({.Name = "Distant"}), 0, {7, 0}, "Distant");
    journal_->ExecutionContext().Invoke(TGuarded<TCreature>(Creature(1)), &TCreature::ApplyFrightened, 2);
    Act("spells.CastRemoveFear", 0, {.Choose = {"Ally"},
        .Changes = {{"Wizard.actions.count", 2}, {"Ally.conditions.Frightened", "<removed>"},
                    {"Ally.conditions.Frightened.value", "<removed>"}}});
    EXPECT_EQ(io_.Candidates("target"), (std::vector<std::string>{"Wizard", "Ally"}));
}

}
