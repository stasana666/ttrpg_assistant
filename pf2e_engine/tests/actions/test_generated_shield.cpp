#include <pf2e_engine/actions/lifecycle_execution.h>
#include <pf2e_engine/inventory/shield_actions.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>

#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

TShield Shield(bool raised = true, int hitpoints = 20, bool wielded = true)
{
    return TShield::Create({.Name = "Steel shield", .ArmorClassBonus = 2, .Hardness = 5, .MaximumHitpoints = 20,
                            .Hitpoints = hitpoints, .BrokenThreshold = 10, .Wielded = wielded, .Raised = raised});
}

TCreature::TInitialValues Fighter(EDamageType type = EDamageType::Slashing)
{
    TIdCollection<TWeapon> weapons;
    weapons.Add(TWeapon::Create({.Name = "Blade", .BaseDiceSize = 12, .AttackBonusBaseline = Fixed(10),
        .DamageModifierBaseline = Fixed(0), .DamageType = type, .Reach = 1, .Features = Features({"abilities.WeaponStrike"})}));
    return {.Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 100}), .Current = 100}),
            .ArmorClassBaseline = Fixed(15), .Weapons = std::move(weapons), .Actions = TResource(3),
            .Reactions = TResource(1)};
}

TCreature::TInitialValues Bearer(TShield shield = Shield(), int current = 100, int temporary = 0)
{
    auto bearer = Fighter();
    bearer.Shield = std::move(shield);
    bearer.Features = Features({"shield_actions.ShieldActions", "shield_actions.ShieldBlockTraining"});
    bearer.Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 100}), .Current = current,
                                           .Temporary = temporary});
    return bearer;
}

class TGeneratedShieldTest : public TBattleFixture {
protected:
    void SetUp() override
    {
        shield_rules_ = std::make_shared<TAbilityRules>();
        RegisterShieldActionsRules(*shield_rules_);
        StartBattle(shield_rules_, 6);
    }

    TPlayer& Add(std::string name, TPosition position, TCreature::TInitialValues initial)
    {
        initial.Name = name;
        return Join(TCreature::Create(initial), static_cast<int>(creatures_.size()), position, std::move(name));
    }

    void Strike(TStep step, int natural = 10, int damage = 9)
    {
        step.Rolls.insert(step.Rolls.begin(), std::pair{20, natural});
        if (damage > 0) {
            step.Rolls.insert(step.Rolls.begin() + 1, std::pair{12, damage});
        }
        step.Choose.insert(step.Choose.begin(), "Bearer");
        step.Changes.emplace("Fighter.actions.count", Read(0).Actions().Count() - 1);
        step.Changes.emplace("Fighter.attacks_made", Read(0).AttacksMade() + 1);
        Act("abilities.Strike", 0, step);
    }

    std::shared_ptr<TAbilityRules> shield_rules_;
};

const TPick kBlock(1, "UseShieldBlock");

TEST_F(TGeneratedShieldTest, DeclaredSteelShieldDefaults)
{
    const auto shield = CreateSteelShield();
    EXPECT_EQ(shield.Name(), "Steel shield");
    EXPECT_EQ(shield.Hitpoints(), 20);
    EXPECT_EQ(shield.MaximumHitpoints(), 20);
    EXPECT_TRUE(shield.CanRaise());
    EXPECT_FALSE(shield.IsRaised());
}

TEST_F(TGeneratedShieldTest, BlockRejectsZeroDamageAndInvalidShields)
{
    Add("Bearer", {0, 0}, Bearer());
    auto execution = journal_->ExecutionContext();
    EXPECT_THROW(execution.Invoke(Creature(0).Shield(), &TShield::Block, 0), std::runtime_error);
    execution.AssignGuarded(Creature(0).Shield(), TShield::Create({.Hardness = -1, .MaximumHitpoints = 20,
        .Hitpoints = 20, .BrokenThreshold = 10, .Wielded = true, .Raised = true}),
        TGuarded<TCreature>(Creature(0)), "shield");
    EXPECT_FALSE(Read(0).Shield().IsValid());
    EXPECT_FALSE(Read(0).Shield().CanRaise());
    EXPECT_THROW(execution.Invoke(Creature(0).Shield(), &TShield::Block, 9), std::runtime_error);
}

TEST_F(TGeneratedShieldTest, RaiseShieldPaysOneActionAndAddsACircumstanceBonus)
{
    Add("Bearer", {0, 0}, Bearer(Shield(false)));
    EXPECT_EQ(Read(0).ArmorClass(), 15);
    Act("shield_actions.RaiseShield", 0, {.Changes = {{"Bearer.shield.raised", true}, {"Bearer.actions.count", 2}}});
    EXPECT_EQ(Read(0).ArmorClass(), 17);
    EXPECT_EQ(Read(0).ArmorClassWithModifiers(TCheckModifiers::Create({
        .StatusBonus = 1, .CircumstanceBonus = 3, .CircumstancePenalty = 1})), 18);
}

TEST_F(TGeneratedShieldTest, MissingBrokenAndUnwieldedShieldsCannotRaise)
{
    Add("Bearer", {0, 0}, Bearer(Shield(false, 10)));
    EXPECT_FALSE(Available("shield_actions.RaiseShield"));
    auto execution = journal_->ExecutionContext();
    execution.AssignGuarded(Creature(0).Shield(), Shield(false, 20, false), TGuarded<TCreature>(Creature(0)), "shield");
    EXPECT_FALSE(Available("shield_actions.RaiseShield"));
    execution.AssignGuarded(Creature(0).Shield(), TShield::Create({}), TGuarded<TCreature>(Creature(0)), "shield");
    EXPECT_FALSE(Available("shield_actions.RaiseShield"));
}

TEST_F(TGeneratedShieldTest, ArmorAndShieldMergeTheirOwnBonusTypesWithExplicitModifiers)
{
    const auto creature = TCreature::Create({
        .Level = 1,
        .Characteristic = TAbilityScores::Create({.Dexterity = TAbilityScore::Create({.Value = 14})}),
        .Proficiency = TProficiency::Create({.ArmorCategory = std::map<EArmorCategory, TProficiencyValue>{{
            EArmorCategory::Light, TProficiencyValue::Create({.Rank = EProficiencyRank::Trained})}}}),
        .Armor = TArmor::Create({.Category = EArmorCategory::Light, .ArmorClassBonus = 2, .DexterityCap = 3}),
        .Shield = Shield(),
    });
    EXPECT_EQ(creature.ArmorClass(), 19);
    EXPECT_EQ(creature.ArmorClassWithModifiers(TCheckModifiers::Create({.ItemBonus = 3, .CircumstanceBonus = 1})), 20);
    EXPECT_EQ(creature.ArmorClassWithModifiers(TCheckModifiers::Create({
        .ItemBonus = 1, .StatusPenalty = 2, .CircumstanceBonus = 3})), 18);
}

TEST_F(TGeneratedShieldTest, BlockAppliesResistanceThenHardnessThenTemporaryHitpoints)
{
    Add("Fighter", {0, 0}, Fighter());
    auto bearer = Bearer(Shield(), 100, 1);
    bearer.Resistances = {{EDamageType::Slashing, 2}};
    Add("Bearer", {1, 0}, bearer);
    Strike({.Choose = {kBlock}, .Changes = {{"Bearer.shield.hitpoints", 18}, {"Bearer.hitpoints.current", 99},
                                            {"Bearer.hitpoints.temporary", 0}, {"Bearer.reactions.count", 0}}});
}

TEST_F(TGeneratedShieldTest, HardnessCanPreventAllDamageWithoutDamagingTheShield)
{
    Add("Fighter", {0, 0}, Fighter());
    Add("Bearer", {1, 0}, Bearer());
    Strike({.Choose = {kBlock}, .Changes = {{"Bearer.reactions.count", 0}}}, 10, 4);
}

TEST_F(TGeneratedShieldTest, DecliningKeepsTheShieldAndTakesTheFullDamage)
{
    Add("Fighter", {0, 0}, Fighter());
    auto bearer = Bearer(Shield(), 100, 1);
    bearer.Resistances = {{EDamageType::Slashing, 2}};
    Add("Bearer", {1, 0}, bearer);
    Strike({.Choose = {TPick(1, "Decline")},
            .Changes = {{"Bearer.hitpoints.current", 94}, {"Bearer.hitpoints.temporary", 0}}});
}

TEST_F(TGeneratedShieldTest, ABrokenShieldLosesItsBonusAndCannotBlockAgain)
{
    Add("Fighter", {0, 0}, Fighter());
    Add("Bearer", {1, 0}, Bearer(Shield(true, 12)));
    Strike({.Choose = {kBlock}, .Changes = {{"Bearer.shield.hitpoints", 8}, {"Bearer.shield.raised", false},
                                            {"Bearer.hitpoints.current", 96}, {"Bearer.reactions.count", 0}}});
    EXPECT_TRUE(Read(1).Shield().IsBroken());
    EXPECT_EQ(Read(1).ArmorClass(), 15);
    EXPECT_FALSE(Available("shield_actions.RaiseShield", 1));
    journal_->ExecutionContext().Invoke(TGuarded<TCreature>(Creature(1)), &TCreature::BeginTurn);
    Strike({.Changes = {{"Bearer.hitpoints.current", 93}}}, 10, 3);
}

TEST_F(TGeneratedShieldTest, ACriticalStrikeCanDestroyTheShieldAndPassTheRemainder)
{
    Add("Fighter", {0, 0}, Fighter());
    Add("Bearer", {1, 0}, Bearer(Shield(true, 15)));
    Strike({.Choose = {kBlock}, .Changes = {{"Bearer.shield.hitpoints", 0}, {"Bearer.shield.raised", false},
                                            {"Bearer.hitpoints.current", 81}, {"Bearer.reactions.count", 0}}}, 20, 12);
    EXPECT_TRUE(Read(1).Shield().IsDestroyed());
}

TEST_F(TGeneratedShieldTest, EnergyDamageIsNotOfferedTheBlock)
{
    Add("Fighter", {0, 0}, Fighter(EDamageType::Fire));
    Add("Bearer", {1, 0}, Bearer());
    Strike({.Changes = {{"Bearer.hitpoints.current", 91}}});
}

TEST_F(TGeneratedShieldTest, ResistanceThatStopsAllDamageSkipsTheBlock)
{
    Add("Fighter", {0, 0}, Fighter());
    auto bearer = Bearer();
    bearer.Resistances = {{EDamageType::Slashing, 20}};
    Add("Bearer", {1, 0}, bearer);
    Strike({});
}

TEST_F(TGeneratedShieldTest, TheRaisedShieldBonusCanTurnTheStrikeIntoAMiss)
{
    Add("Fighter", {0, 0}, Fighter());
    Add("Bearer", {1, 0}, Bearer());
    Strike({}, 5, 0);
}

TEST_F(TGeneratedShieldTest, OnlyTheAttackedBearerIsOfferedTheBlock)
{
    Add("Fighter", {0, 0}, Fighter());
    Add("Bearer", {1, 0}, Bearer());
    Add("Other", {0, 1}, Bearer());
    Strike({.Choose = {kBlock}, .Changes = {{"Bearer.shield.hitpoints", 16}, {"Bearer.hitpoints.current", 96},
                                            {"Bearer.reactions.count", 0}}});
}

TEST_F(TGeneratedShieldTest, BlockCompletesBeforeTheRemainderKnocksOutAndDropsTheShield)
{
    Add("Fighter", {0, 0}, Fighter());
    auto bearer = Bearer(Shield(), 3);
    bearer.ZeroHpRule = EZeroHpRule::Dying;
    Add("Bearer", {1, 0}, bearer);
    Strike({.Choose = {kBlock}, .Changes = {{"Bearer.shield.hitpoints", 16}, {"Bearer.shield.wielded", false},
                                            {"Bearer.shield.raised", false}, {"Bearer.hands.count", 1},
                                            {"Bearer.hitpoints.current", 0}, {"Bearer.conditions.Dying", true},
                                            {"Bearer.conditions.Dying.value", 1}, {"Bearer.conditions.Unconscious", true},
                                            {"Bearer.conditions.Prone", true}, {"Bearer.reactions.count", 0}}});
}

TEST_F(TGeneratedShieldTest, TurnStartExpirationLowersOnlyTheActorsShield)
{
    Add("First", {0, 0}, Bearer());
    Add("Second", {1, 0}, Bearer());
    journal_->ExecutionContext().Invoke(TGuarded<TCreature>(Creature(0)), &TCreature::ExpireTurnStartEffects);
    EXPECT_FALSE(Read(0).Shield().IsRaised());
    EXPECT_TRUE(Read(1).Shield().IsRaised());
    EXPECT_EQ(Read(0).ArmorClass(), 15);
}

}
