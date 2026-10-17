#include <pf2e_engine/inventory/generated_strike_activities.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

TWeapon::TInitialValues Blade(int die = 6, std::initializer_list<std::string> features = {"abilities.WeaponStrike"})
{
    return {.Name = "Blade", .BaseDiceSize = die, .AttackBonusBaseline = Fixed(9),
            .DamageModifierBaseline = Fixed(4), .DamageType = EDamageType::Piercing,
            .Category = EWeaponCategory::Simple, .Reach = 1, .Features = Features(features)};
}

TWeapon::TInitialValues Jaws(int reach = 1)
{
    auto jaws = Blade();
    jaws.Name = "Jaws";
    jaws.DamageModifierBaseline = Fixed(2);
    jaws.Category = EWeaponCategory::Unarmed;
    jaws.Reach = reach;
    return jaws;
}

TIdCollection<TWeapon> Weapons(std::initializer_list<TWeapon::TInitialValues> values)
{
    TIdCollection<TWeapon> result;
    for (const auto& value : values) {
        result.Add(TWeapon::Create(value));
    }
    return result;
}

TCreature::TInitialValues Armed(std::initializer_list<TWeapon::TInitialValues> weapons = {Blade()})
{
    return {.Weapons = Weapons(weapons)};
}

TCreature::TInitialValues Wolf(bool pack = false, int reach = 1)
{
    auto wolf = TCreature::TInitialValues{.NaturalWeapons = Weapons({Jaws(reach)})};
    if (pack) {
        wolf.Features = Features({"abilities.PackAttack"});
    }
    return wolf;
}

TCreature::TInitialValues Vicious(std::initializer_list<TWeapon::TInitialValues> weapons = {Blade(6, {})})
{
    auto fighter = Armed(weapons);
    fighter.Class = TClass::Create({.Name = "Fighter", .Features = Features({"abilities.ViciousSwing"})});
    return fighter;
}

THitPoints Hitpoints(int current, int temporary = 0)
{
    return THitPoints::Create({.Maximum = Statistic::Create({.Base = 100}), .Current = current, .Temporary = temporary});
}

class TGeneratedStrikeTest : public TBattleFixture {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedStrikeActivitiesRules(*rules);
        StartBattle(std::move(rules), 8);
    }

    TPlayer& Add(std::string name, TPosition position, int team, TCreature::TInitialValues initial = {})
    {
        initial.Name = name;
        if (!initial.Hitpoints) {
            initial.Hitpoints = Hitpoints(100);
        }
        if (!initial.ArmorClassBaseline) {
            initial.ArmorClassBaseline = Fixed(15);
        }
        if (!initial.Actions) {
            initial.Actions = TResource(3);
        }
        return Join(TCreature::Create(initial), team, position, std::move(name));
    }
};

TEST_F(TGeneratedStrikeTest, NaturalWeaponsStrikeWithTheirOwnBaselines)
{
    Add("Wolf", {0, 0}, 0, Wolf());
    Add("Target", {1, 0}, 1);
    Act("abilities.Strike", 0, {.Choose = {"Target"}, .Rolls = {{20, 6}, {6, 3}},
        .Changes = {{"Target.hitpoints.current", 95}, {"Wolf.actions.count", 2}, {"Wolf.attacks_made", 1}}});
}

TEST_F(TGeneratedStrikeTest, KilledTargetIsNoLongerOfferedAndTheNextStrikeTakesThePenalty)
{
    Add("Fighter", {0, 0}, 0, Armed({Blade(12)}));
    Add("First", {1, 0}, 1, {.Hitpoints = Hitpoints(5)});
    Add("Second", {0, 1}, 1);
    Act("abilities.Strike", 0, {.Choose = {"First"}, .Rolls = {{20, 10}, {12, 7}},
        .Changes = {{"First.hitpoints.current", 0}, {"Fighter.actions.count", 2}, {"Fighter.attacks_made", 1}}});
    Act("abilities.Strike", 0, {.Choose = {"Second"}, .Rolls = {{20, 15}, {12, 7}},
        .Changes = {{"Second.hitpoints.current", 89}, {"Fighter.actions.count", 1}, {"Fighter.attacks_made", 2}}});
    EXPECT_EQ(io_.Candidates(), (std::vector<std::string>{"Second"}));
}

TEST_F(TGeneratedStrikeTest, DegreesOfSuccessFollowTheNaturalRollAdjustments)
{
    struct TCase {
        int Natural;
        int ArmorClass;
        int Damage;
    };
    const std::vector<TCase> cases = {
        {6, 15, 7}, {5, 15, 0}, {16, 15, 14}, {1, 15, 0}, {20, 35, 7}, {20, 40, 0}, {1, 0, 7}};
    const std::vector<TPosition> around = {{2, 2}, {2, 3}, {2, 4}, {3, 2}, {3, 4}, {4, 2}, {4, 3}};
    Add("Fighter", {3, 3}, 0, Armed());
    for (size_t index = 0; index < cases.size(); ++index) {
        Add("Target " + std::to_string(index), around[index], 1, {.ArmorClassBaseline = Fixed(cases[index].ArmorClass)});
    }
    for (size_t index = 0; index < cases.size(); ++index) {
        const auto target = "Target " + std::to_string(index);
        SCOPED_TRACE(target);
        TStep step{.Choose = {target}, .Rolls = {{20, cases[index].Natural}},
                   .Changes = {{"Fighter.actions.count", 2}, {"Fighter.attacks_made", 1}}};
        if (cases[index].Damage > 0) {
            step.Rolls.emplace_back(6, 3);
            step.Changes.emplace(target + ".hitpoints.current", 100 - cases[index].Damage);
        }
        Check(Option("abilities.Strike"), step);
    }
}

TEST_F(TGeneratedStrikeTest, FatalReplacesTheDiceAndDeadlyAddsOneDieOnlyOnCriticalHits)
{
    auto blade = Blade();
    blade.DamageDiceCount = 2;
    blade.Traits.emplace();
    blade.Traits->Set(TWeaponTraitFatal{.Die = EDieSize::D10});
    blade.Traits->Set(TWeaponTraitDeadly{.Die = EDieSize::D8});
    Add("Fighter", {0, 0}, 0, Armed({blade}));
    Add("Target", {1, 0}, 1);
    const auto option = Option("abilities.Strike");
    Check(option, {.Choose = {"Target"}, .Rolls = {{20, 10}, {6, 3}, {6, 5}},
        .Changes = {{"Target.hitpoints.current", 88}, {"Fighter.actions.count", 2}, {"Fighter.attacks_made", 1}}});
    Check(option, {.Choose = {"Target"}, .Rolls = {{20, 20}, {10, 3}, {10, 5}, {10, 7}, {8, 6}},
        .Changes = {{"Target.hitpoints.current", 63}, {"Fighter.actions.count", 2}, {"Fighter.attacks_made", 1}}});
}

TEST_F(TGeneratedStrikeTest, MinimumDamageIsAppliedBeforeCriticalDoubling)
{
    auto blade = Blade();
    blade.DamageModifierBaseline = Fixed(-8);
    Add("Fighter", {0, 0}, 0, Armed({blade}));
    Add("Target", {1, 0}, 1);
    Act("abilities.Strike", 0, {.Choose = {"Target"}, .Rolls = {{20, 20}, {6, 1}},
        .Changes = {{"Target.hitpoints.current", 98}, {"Fighter.actions.count", 2}, {"Fighter.attacks_made", 1}}});
}

TEST_F(TGeneratedStrikeTest, PackAttackNeedsTwoOtherLivingAlliesWithinTheirReach)
{
    Add("Wolf A", {0, 0}, 0, Wolf(true));
    Add("Target", {1, 0}, 1);
    Add("Wolf B", {1, 1}, 0, Wolf());
    auto defeated = Wolf();
    defeated.Hitpoints = Hitpoints(0);
    Add("Defeated", {2, 0}, 0, defeated);
    Act("abilities.Strike", 0, {.Choose = {"Target"}, .Rolls = {{20, 10}, {6, 3}},
        .Changes = {{"Target.hitpoints.current", 95}, {"Wolf A.actions.count", 2}, {"Wolf A.attacks_made", 1}}});
    Add("Long reach", {3, 0}, 0, Wolf(false, 2));
    Act("abilities.Strike", 0, {.Choose = {"Target"}, .Rolls = {{20, 15}, {6, 3}, {4, 2}},
        .Changes = {{"Target.hitpoints.current", 88}, {"Wolf A.actions.count", 1}, {"Wolf A.attacks_made", 2}}});
}

TEST_F(TGeneratedStrikeTest, AnAllyCanBeTargetedButDoesNotCountAsAPackAlly)
{
    Add("Wolf A", {0, 0}, 0, Wolf(true));
    Add("Friend", {1, 0}, 0, Wolf());
    Add("Other", {1, 1}, 0, Wolf());
    Add("Enemy", {5, 5}, 1);
    Act("abilities.Strike", 0, {.Choose = {"Friend"}, .Rolls = {{20, 10}, {6, 3}},
        .Changes = {{"Friend.hitpoints.current", 95}, {"Wolf A.actions.count", 2}, {"Wolf A.attacks_made", 1}}});
}

TEST_F(TGeneratedStrikeTest, PackDamageIsCombinedBeforeDefensesAndTemporaryHitpoints)
{
    Add("Wolf A", {0, 0}, 0, Wolf(true));
    Add("Target", {1, 0}, 1, {.Hitpoints = Hitpoints(100, 4), .Resistances = {{{EDamageType::Piercing, 5}}},
                              .Vulnerabilities = {{{EDamageType::Piercing, 3}}}});
    Add("Wolf B", {2, 0}, 0, Wolf());
    Add("Wolf C", {1, 1}, 0, Wolf());
    const auto option = Option("abilities.Strike");
    Check(option, {.Choose = {"Target"}, .Rolls = {{20, 10}, {6, 3}, {4, 2}},
        .Changes = {{"Target.hitpoints.current", 99}, {"Target.hitpoints.temporary", 0},
                    {"Wolf A.actions.count", 2}, {"Wolf A.attacks_made", 1}}});
    Check(option, {.Choose = {"Target"}, .Rolls = {{20, 20}, {6, 3}, {4, 2}},
        .Changes = {{"Target.hitpoints.current", 92}, {"Target.hitpoints.temporary", 0},
                    {"Wolf A.actions.count", 2}, {"Wolf A.attacks_made", 1}}});
}

TEST_F(TGeneratedStrikeTest, ImmunityAndFullResistanceLeaveTemporaryHitpoints)
{
    Add("Fighter", {0, 0}, 0, Armed());
    Add("Immune", {1, 0}, 1, {.Hitpoints = Hitpoints(100, 5), .Immunities = {{EDamageType::Piercing}},
                              .Vulnerabilities = {{{EDamageType::Piercing, 100}}}});
    Add("Resistant", {0, 1}, 1, {.Hitpoints = Hitpoints(100, 5), .Resistances = {{{EDamageType::Piercing, 100}}}});
    Act("abilities.Strike", 0, {.Choose = {"Immune"}, .Rolls = {{20, 10}, {6, 3}},
        .Changes = {{"Fighter.actions.count", 2}, {"Fighter.attacks_made", 1}}});
    Act("abilities.Strike", 0, {.Choose = {"Resistant"}, .Rolls = {{20, 15}, {6, 3}},
        .Changes = {{"Fighter.actions.count", 1}, {"Fighter.attacks_made", 2}}});
}

TEST_F(TGeneratedStrikeTest, StrikeIsHiddenAndRejectedWhenNoTargetIsInReach)
{
    Add("Fighter", {0, 0}, 0, Armed());
    Add("Target", {1, 0}, 1);
    const auto option = Option("abilities.Strike");
    journal_->MovePlayer(&Player(1), {5, 5});
    EXPECT_TRUE(Options("abilities.Strike").empty());
    Act(option, {.Result = EAbilityExecutionResult::Unavailable});
}

TEST_F(TGeneratedStrikeTest, SharedActivityCountsTwoAttacksOnAMissAndOnlyThePrimaryActionPays)
{
    auto fighter = Armed({Blade(6, {"generated_strike_activities.HeavyWeapon"})});
    fighter.AttacksMade = 1;
    fighter.Hands = TResource(2);
    Add("Fighter", {0, 0}, 0, fighter);
    Add("Target", {1, 0}, 1, {.ArmorClassBaseline = Fixed(20)});
    Act("generated_strike_activities.HeavyStrike", 0, {.Choose = {"Target"}, .Rolls = {{20, 15}},
        .Changes = {{"Fighter.actions.count", 1}, {"Fighter.attacks_made", 3}, {"Fighter.hands.count", 0}}});
}

TEST_F(TGeneratedStrikeTest, ActivityExtraDiceUseFatalSizeAndCriticalDoubling)
{
    auto blade = Blade(6, {"generated_strike_activities.HeavyWeapon"});
    blade.Traits.emplace();
    blade.Traits->Set(TWeaponTraitFatal{.Die = EDieSize::D10});
    Add("Fighter", {0, 0}, 0, Armed({blade}));
    Add("Target", {1, 0}, 1);
    Act("generated_strike_activities.HeavyStrike", 0, {.Choose = {"Target"},
        .Rolls = {{20, 20}, {10, 6}, {10, 7}, {10, 3}},
        .Changes = {{"Target.hitpoints.current", 63}, {"Fighter.actions.count", 1}, {"Fighter.attacks_made", 2},
                    {"Fighter.hands.count", 1}}});
}

TEST_F(TGeneratedStrikeTest, ActivityCanIgnoreTheMultipleAttackPenaltyWithoutCountingAnAttack)
{
    auto fighter = Armed({Blade(6, {"generated_strike_activities.HeavyWeapon"})});
    fighter.AttacksMade = 2;
    fighter.Reactions = TResource(1);
    Add("Fighter", {0, 0}, 0, fighter);
    Add("Target", {1, 0}, 1);
    Act("generated_strike_activities.UnpenalizedStrike", 0, {.Choose = {"Target"}, .Rolls = {{20, 6}, {6, 3}},
        .Changes = {{"Target.hitpoints.current", 93}, {"Fighter.reactions.count", 0}, {"Fighter.hands.count", 1}}});
}

TEST_F(TGeneratedStrikeTest, ViciousSwingChoosesAmongSameNameWeaponsAndNaturalAttacks)
{
    auto jaws = Blade(6, {});
    jaws.Name = "Jaws";
    auto fighter = Vicious({Blade(4, {}), Blade(12, {})});
    fighter.NaturalWeapons = Weapons({jaws});
    Add("Fighter", {0, 0}, 0, fighter);
    Add("Target", {1, 0}, 1);
    ASSERT_EQ(battle_->AvailableActions(Player(0)).size(), 1u);
    const auto option = Option("abilities.UseViciousSwing");
    EXPECT_TRUE(option.Source.StartsWith(TAbilitySourcePath{}.Field("class")));
    Check(option, {.Choose = {"Target", "Blade (2)"}, .Rolls = {{20, 6}, {12, 7}, {12, 8}},
        .Changes = {{"Target.hitpoints.current", 81}, {"Fighter.actions.count", 1}, {"Fighter.flourishes.count", 0},
                    {"Fighter.attacks_made", 2}}});
    EXPECT_EQ(io_.Candidates(), (std::vector<std::string>{"Blade (1)", "Blade (2)", "Jaws"}));
    Check(option, {.Choose = {"Target", "Jaws"}, .Rolls = {{20, 6}, {6, 3}, {6, 4}},
        .Changes = {{"Target.hitpoints.current", 89}, {"Fighter.actions.count", 1}, {"Fighter.flourishes.count", 0},
                    {"Fighter.attacks_made", 2}}});
}

TEST_F(TGeneratedStrikeTest, ViciousSwingTakesTheExistingPenaltyAndCountsTwoAttacksOnAMiss)
{
    auto fighter = Vicious();
    fighter.AttacksMade = 1;
    Add("Fighter", {0, 0}, 0, fighter);
    Add("Target", {1, 0}, 1, {.ArmorClassBaseline = Fixed(20)});
    Act("abilities.UseViciousSwing", 0, {.Choose = {"Target", "Blade"}, .Rolls = {{20, 15}},
        .Changes = {{"Fighter.actions.count", 1}, {"Fighter.flourishes.count", 0}, {"Fighter.attacks_made", 3}}});
}

TEST_F(TGeneratedStrikeTest, ViciousSwingExtraDiceGrowAtTenthAndEighteenthLevel)
{
    const std::vector<std::pair<int, int>> levels = {{1, 1}, {9, 1}, {10, 2}, {17, 2}, {18, 3}};
    const std::vector<TPosition> around = {{2, 2}, {2, 3}, {2, 4}, {3, 2}, {3, 4}};
    Add("Target", {3, 3}, 1);
    for (size_t index = 0; index < levels.size(); ++index) {
        auto fighter = Vicious();
        fighter.Level = levels[index].first;
        Add("Fighter " + std::to_string(index), around[index], 0, fighter);
    }
    for (size_t index = 0; index < levels.size(); ++index) {
        const auto [level, extra] = levels[index];
        const auto fighter = "Fighter " + std::to_string(index);
        SCOPED_TRACE(level);
        TStep step{.Choose = {"Target", "Blade"}, .Rolls = {{20, 6}},
                   .Changes = {{"Target.hitpoints.current", 100 - 3 * (extra + 1) - 4}, {fighter + ".actions.count", 1},
                               {fighter + ".flourishes.count", 0}, {fighter + ".attacks_made", 2}}};
        step.Rolls.insert(step.Rolls.end(), extra + 1, {6, 3});
        Check(Option("abilities.UseViciousSwing", static_cast<int>(index) + 1), step);
    }
}

TEST_F(TGeneratedStrikeTest, ViciousSwingUsesOneFlourishPerRound)
{
    Add("Fighter", {0, 0}, 0, Vicious());
    Add("Target", {1, 0}, 1);
    const auto option = Option("abilities.UseViciousSwing");
    auto context = journal_->ExecutionContext();
    auto& fighter = Creature(0);
    context.SetResourceCount(fighter.Flourishes(), 0, TGuarded<TCreature>(fighter), "flourishes");
    Act(option, {.Result = EAbilityExecutionResult::Unavailable});
    context.Invoke(TGuarded<TCreature>(fighter), &TCreature::BeginTurn);
    EXPECT_EQ(Read(0).Flourishes().Count(), 0);
    context.Invoke(TGuarded<TCreature>(fighter), &TCreature::BeginRound);
    Act(option, {.Choose = {"Target", "Blade"}, .Rolls = {{20, 1}},
        .Changes = {{"Fighter.actions.count", 1}, {"Fighter.flourishes.count", 0}, {"Fighter.attacks_made", 2}}});
}

}
