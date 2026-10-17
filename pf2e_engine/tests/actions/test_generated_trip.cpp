#include <pf2e_engine/inventory/movement.h>
#include <pf2e_engine/inventory/shield_actions.h>
#include <pf2e_engine/inventory/trip.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

template <class... TPayload>
TVariantMap<EWeaponTraitKind, TWeaponTrait> Traits(TPayload... payloads)
{
    TVariantMap<EWeaponTraitKind, TWeaponTrait> traits;
    (traits.Set(payloads), ...);
    return traits;
}

TWeapon Jaws(bool knockdown = true, bool strike = true, TVariantMap<EWeaponTraitKind, TWeaponTrait> traits = {})
{
    auto features = Features({});
    if (knockdown) {
        features.Add(TFeatureSelection("trip.Knockdown"));
    }
    if (strike) {
        features.Add(TFeatureSelection("abilities.WeaponStrike"));
    }
    return TWeapon::Create({.Name = "Jaws", .BaseDiceSize = 6, .AttackBonusBaseline = Fixed(10),
        .DamageModifierBaseline = Fixed(0), .DamageType = EDamageType::Piercing, .Category = EWeaponCategory::Unarmed,
        .Reach = 1, .HandsRequired = 0, .Traits = std::move(traits), .Features = std::move(features)});
}

TIdCollection<TWeapon> Weapons(std::initializer_list<TWeapon> weapons)
{
    TIdCollection<TWeapon> result;
    for (const auto& weapon : weapons) {
        result.Add(weapon);
    }
    return result;
}

TShield RaisedShield()
{
    return TShield::Create({.Name = "Shield", .ArmorClassBonus = 2, .Hardness = 2, .MaximumHitpoints = 20,
                            .Hitpoints = 20, .BrokenThreshold = 10, .Wielded = true, .Raised = true});
}

TCreature::TInitialValues Profile()
{
    TCreature::TInitialValues initial;
    initial.Size = ECreatureSize::Medium;
    initial.Movement = 5;
    initial.SaveBaselines = {{ESavethrow::Reflex, Fixed(5)}};
    initial.SkillBaselines = {{ESkill::Athletics, Fixed(10)}};
    initial.Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 100}), .Current = 100});
    initial.ArmorClassBaseline = Fixed(15);
    initial.NaturalWeapons = Weapons({Jaws()});
    initial.Features = Features({"abilities.BasicStrike", "trip.TripActions", "movement.MovementActions"});
    initial.Actions = TResource(10);
    initial.Reactions = TResource(1);
    initial.Flourishes = TResource(1);
    initial.Hands = TResource(1);
    return initial;
}

TCreature::TInitialValues Blocker(TCreature::TInitialValues initial = Profile())
{
    initial.Shield = RaisedShield();
    initial.Features = Features({"shield_actions.ShieldBlockTraining"});
    return initial;
}

class TGeneratedTripTest : public TBattleFixture {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterTripRules(*rules);
        RegisterMovementRules(*rules);
        RegisterShieldActionsRules(*rules);
        StartBattle(std::move(rules), 8);
    }

    TPlayer& Add(TPosition position, TCreature::TInitialValues initial = Profile())
    {
        const int id = static_cast<int>(creatures_.size());
        initial.Name = "Actor " + std::to_string(id);
        return Join(TCreature::Create(initial), id, position);
    }

    void Strike(int natural = 10, int damage = 2, int target = 1)
    {
        const auto name = "Actor " + std::to_string(target);
        const auto& actor = Read(0);
        TStep step{.Choose = {name}, .Rolls = {{20, natural}},
                   .Changes = {{"Actor 0.actions.count", actor.Actions().Count() - 1},
                               {"Actor 0.attacks_made", actor.AttacksMade() + 1}}};
        if (damage > 0) {
            step.Rolls.emplace_back(6, damage);
            step.Changes.emplace(name + ".hitpoints.current", Read(target).Hitpoints().Current() - damage);
        }
        Act("abilities.Strike", 0, step);
    }

    bool HasRecord(int actor = 0)
    {
        return battle_->History().Find(Player(actor), TRecordStrikeResult::kId).has_value();
    }
};

TEST_F(TGeneratedTripTest, TripResolvesAllFourOutcomes)
{
    Add({1, 1});
    Add({2, 1});
    const auto option = Option("trip.UseTrip");
    struct TCase {
        int Natural;
        std::map<std::string, TField> Changes;
    };
    for (auto& test : std::vector<TCase>{
             {20, {{"Actor 1.conditions.Prone", true}, {"Actor 1.hitpoints.current", 94}}},
             {5, {{"Actor 1.conditions.Prone", true}}},
             {2, {}},
             {1, {{"Actor 0.conditions.Prone", true}}}}) {
        SCOPED_TRACE(test.Natural);
        TStep step{.Choose = {"Actor 1"}, .Rolls = {{20, test.Natural}}, .Changes = std::move(test.Changes)};
        if (test.Natural == 20) {
            step.Rolls.emplace_back(6, 6);
        }
        step.Changes.emplace("Actor 0.actions.count", 9);
        step.Changes.emplace("Actor 0.attacks_made", 1);
        Check(option, step);
        EXPECT_FALSE(HasRecord());
    }
}

TEST_F(TGeneratedTripTest, TripTakesTheNonAgilePenaltyAfterAnAgileStrike)
{
    auto actor = Profile();
    actor.NaturalWeapons = Weapons({Jaws(true, true, Traits(TWeaponTraitAgile{}))});
    Add({1, 1}, actor);
    Add({2, 1});
    Strike();
    Act("trip.UseTrip", 0, {.Choose = {"Actor 1"}, .Rolls = {{20, 9}},
        .Changes = {{"Actor 0.actions.count", 8}, {"Actor 0.attacks_made", 2}}});
    EXPECT_FALSE(HasRecord());
}

TEST_F(TGeneratedTripTest, TripNeedsAFreeHandAndATargetInBodyReachAtMostOneSizeLarger)
{
    auto actor = Profile();
    actor.Weapons = Weapons({TWeapon::Create({.Name = "Long weapon", .BaseDiceSize = 6, .Reach = 3})});
    actor.Hands = TResource(0);
    Add({1, 1}, actor);
    Add({3, 1});
    EXPECT_TRUE(Options("trip.UseTrip").empty());
    auto execution = journal_->ExecutionContext();
    execution.SetResourceCount(Creature(0).Hands(), 1, TGuarded<TCreature>(Creature(0)), "hands");
    EXPECT_TRUE(Options("trip.UseTrip").empty());
    journal_->MovePlayer(&Player(1), {2, 1});
    execution.AssignGuarded(Creature(1).Size(), ECreatureSize::Huge, TGuarded<TCreature>(Creature(1)), "size");
    EXPECT_TRUE(Options("trip.UseTrip").empty());
    execution.AssignGuarded(Creature(1).Size(), ECreatureSize::Large, TGuarded<TCreature>(Creature(1)), "size");
    EXPECT_EQ(Options("trip.UseTrip").size(), 1U);
}

TEST_F(TGeneratedTripTest, TenFootBodyReachIncludesTheSecondDiagonal)
{
    auto actor = Profile();
    actor.BodyReach = 2;
    Add({1, 1}, actor);
    Add({3, 3});
    Act("trip.UseTrip", 0, {.Choose = {"Actor 1"}, .Rolls = {{20, 5}},
        .Changes = {{"Actor 1.conditions.Prone", true}, {"Actor 0.actions.count", 9}, {"Actor 0.attacks_made", 1}}});
    const auto rules = TTripRules::Create({});
    EXPECT_TRUE(rules.InReach(1, 1, 3, 3, 2));
    EXPECT_FALSE(rules.InReach(1, 1, 3, 3, 1));
    EXPECT_FALSE(rules.InReach(0, 0, 3, 3, 3));
}

TEST_F(TGeneratedTripTest, CriticalTripDamageGoesThroughResistanceShieldBlockAndTemporaryHitpoints)
{
    Add({1, 1});
    auto target = Blocker();
    target.Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 100}), .Current = 100, .Temporary = 2});
    target.Resistances = {{EDamageType::Bludgeoning, 1}};
    Add({2, 1}, target);
    Act("trip.UseTrip", 0, {.Choose = {"Actor 1", TPick(1, "UseShieldBlock")}, .Rolls = {{20, 20}, {6, 6}},
        .Changes = {{"Actor 1.conditions.Prone", true}, {"Actor 1.hitpoints.current", 99},
                    {"Actor 1.hitpoints.temporary", 0}, {"Actor 1.shield.hitpoints", 17},
                    {"Actor 1.reactions.count", 0}, {"Actor 0.actions.count", 9}, {"Actor 0.attacks_made", 1}}});
}

TEST_F(TGeneratedTripTest, CriticalTripKnocksOutWithCriticalContextWithoutDoublingItsDie)
{
    Add({1, 1});
    auto target = Profile();
    target.Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 100}), .Current = 3});
    target.ZeroHpRule = EZeroHpRule::Dying;
    Add({2, 1}, target);
    Act("trip.UseTrip", 0, {.Choose = {"Actor 1"}, .Rolls = {{20, 20}, {6, 3}},
        .Changes = {{"Actor 1.conditions.Prone", true}, {"Actor 1.conditions.Unconscious", true},
                    {"Actor 1.conditions.Dying", true}, {"Actor 1.conditions.Dying.value", 2},
                    {"Actor 1.hitpoints.current", 0}, {"Actor 0.actions.count", 9}, {"Actor 0.attacks_made", 1}}});
}

TEST_F(TGeneratedTripTest, KnockdownFollowsAHitWithTheSameJawsWithoutFreeHandPenaltyOrAttackCount)
{
    auto actor = Profile();
    actor.Hands = TResource(0);
    actor.AttacksMade = 2;
    Add({1, 1}, actor);
    Add({2, 1});
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
    EXPECT_TRUE(Options("trip.UseTrip").empty());
    Strike(15);
    Act("trip.UseKnockdown", 0, {.Rolls = {{20, 5}},
        .Changes = {{"Actor 1.conditions.Prone", true}, {"Actor 0.actions.count", 8}}});
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
}

TEST_F(TGeneratedTripTest, KnockdownUsesTheExactWeaponThatHit)
{
    auto actor = Profile();
    actor.NaturalWeapons = Weapons({Jaws(true, false), Jaws(true, false)});
    Add({1, 1}, actor);
    Add({2, 1});
    EXPECT_TRUE(Options("abilities.Strike").empty());
    Act("abilities.UseStrike", 0, {.Choose = {"Actor 1", "Jaws (2)"}, .Rolls = {{20, 10}, {6, 2}},
        .Changes = {{"Actor 1.hitpoints.current", 98}, {"Actor 0.actions.count", 9}, {"Actor 0.attacks_made", 1}}});
    const auto options = Options("trip.UseKnockdown");
    ASSERT_EQ(options.size(), 1U);
    EXPECT_TRUE(options[0].Source.StartsWith(TAbilitySourcePath{}.Field("natural_weapons").Entry(1)));
}

TEST_F(TGeneratedTripTest, AMissDoesNotQualifyAndAFailedKnockdownStillUsesTheHit)
{
    Add({1, 1});
    Add({2, 1});
    Strike(2, 0);
    EXPECT_TRUE(HasRecord());
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
    Strike(10);
    Act("trip.UseKnockdown", 0, {.Rolls = {{20, 1}},
        .Changes = {{"Actor 0.conditions.Prone", true}, {"Actor 0.actions.count", 7}}});
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
}

TEST_F(TGeneratedTripTest, KnockdownAddedAfterTheHitDoesNotQualifyThatHit)
{
    auto actor = Profile();
    actor.NaturalWeapons = Weapons({Jaws(false)});
    Add({1, 1}, actor);
    Add({2, 1});
    Strike();
    TTransformator editor(io_);
    auto execution = editor.ExecutionContext();
    const auto weapon = execution.Project(Creature(0).NaturalWeapons(), [](TIdCollection<TWeapon>& values) {
        return TGuarded<TWeapon>(*values.Find(TId<TWeapon>{0}));
    });
    const auto features = execution.Project(weapon, [](TWeapon& value) { return value.Features(); });
    execution.AssignGuarded(features, Features({"abilities.WeaponStrike", "trip.Knockdown"}), weapon, "features");
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
    Strike();
    EXPECT_EQ(Options("trip.UseKnockdown").size(), 1U);
}

TEST_F(TGeneratedTripTest, ANewHitReplacesTheSavedKnockdownInsteadOfRetargetingIt)
{
    Add({1, 1});
    Add({2, 1});
    Add({1, 2});
    Strike();
    const auto stale = Option("trip.UseKnockdown");
    Strike(10, 2, 2);
    Act(stale, {.Result = EAbilityExecutionResult::Unavailable});
    Act("trip.UseKnockdown", 0, {.Rolls = {{20, 5}},
        .Changes = {{"Actor 2.conditions.Prone", true}, {"Actor 0.actions.count", 7}}});
}

TEST_F(TGeneratedTripTest, SavedKnockdownNeedsTheTargetInReachAndTheSameWeapon)
{
    Add({1, 1});
    Add({2, 1});
    Strike();
    const auto option = Option("trip.UseKnockdown");
    TTransformator editor(io_);
    const auto initial = editor.CurrentState();
    editor.MovePlayer(&Player(1), {4, 1});
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
    Act(option, {.Result = EAbilityExecutionResult::Unavailable});
    editor.Undo(initial);
    auto weapons = Read(0).NaturalWeapons();
    weapons.Remove(TId<TWeapon>{0});
    weapons.Add(Jaws());
    editor.ExecutionContext().AssignGuarded(Creature(0).NaturalWeapons(), std::move(weapons),
        TGuarded<TCreature>(Creature(0)), "natural_weapons");
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
    Act(option, {.Result = EAbilityExecutionResult::Unavailable});
    editor.Undo(initial);
    EXPECT_EQ(Options("trip.UseKnockdown").size(), 1U);
}

TEST_F(TGeneratedTripTest, OnlyTheActorsOwnNextActionUsesUpTheHit)
{
    Add({1, 1});
    Add({2, 1});
    Strike();
    Act("movement.UseStep", 1, {.Choose = {TPick::Value(TPosition{3, 1})}, .Changes = {{"Actor 1.position", "TPosition{3,1}"},
        {"Actor 1.actions.count", 9}}});
    Act("movement.UseStep", 1, {.Choose = {TPick::Value(TPosition{2, 1})}, .Changes = {{"Actor 1.position", "TPosition{2,1}"},
        {"Actor 1.actions.count", 8}}});
    EXPECT_EQ(Options("trip.UseKnockdown").size(), 1U);
    Act("movement.UseStep", 0, {.Choose = {TPick::Value(TPosition{1, 2})}, .Changes = {{"Actor 0.position", "TPosition{1,2}"},
        {"Actor 0.actions.count", 8}}});
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
}

TEST_F(TGeneratedTripTest, ViciousSwingDoesNotPublishAStrikeResult)
{
    auto actor = Profile();
    actor.Features = Features({"abilities.BasicStrike", "abilities.ViciousSwing"});
    Add({1, 1}, actor);
    Add({2, 1});
    Act("abilities.UseViciousSwing", 0, {.Choose = {"Actor 1", "Jaws"}, .Rolls = {{20, 10}, {6, 2}, {6, 2}},
        .Changes = {{"Actor 1.hitpoints.current", 96}, {"Actor 0.actions.count", 8}, {"Actor 0.flourishes.count", 0},
                    {"Actor 0.attacks_made", 2}}});
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
}

TEST_F(TGeneratedTripTest, VersatileDamageTypeIsChosenOnlyWhenItDiffers)
{
    auto actor = Profile();
    actor.NaturalWeapons = Weapons({Jaws(false, true, Traits(TWeaponTraitVersatile{EDamageType::Slashing})),
                                    Jaws(false, true, Traits(TWeaponTraitVersatile{EDamageType::Piercing}))});
    Add({1, 1}, actor);
    auto target = Profile();
    target.Immunities = {EDamageType::Piercing};
    target.Vulnerabilities = {{EDamageType::Slashing, 3}};
    Add({2, 1}, target);
    const auto versatile = Option("abilities.Strike");
    const std::map<std::string, TField> paid = {{"Actor 0.actions.count", 9}, {"Actor 0.attacks_made", 1}};
    Check(versatile, {.Choose = {"Actor 1", TPick::Value(EDamageType::Piercing)}, .Rolls = {{20, 10}, {6, 4}},
        .Changes = paid});
    auto slashing = paid;
    slashing.emplace("Actor 1.hitpoints.current", 93);
    Check(versatile, {.Choose = {"Actor 1", TPick::Value(EDamageType::Slashing)}, .Rolls = {{20, 10}, {6, 4}},
        .Changes = slashing});
    Check(Option("abilities.Strike", 0, 1), {.Choose = {"Actor 1"}, .Rolls = {{20, 10}, {6, 4}}, .Changes = paid});
}

TEST_F(TGeneratedTripTest, VersatileStrikePublishesItsResultAfterTheShieldBlock)
{
    auto actor = Profile();
    actor.NaturalWeapons = Weapons({Jaws(true, true, Traits(TWeaponTraitVersatile{EDamageType::Slashing}))});
    Add({1, 1}, actor);
    auto target = Blocker();
    target.Immunities = {EDamageType::Piercing};
    Add({2, 1}, target);
    Act("abilities.Strike", 0, {.Choose = {"Actor 1", TPick::Value(EDamageType::Slashing), TPick(1, "UseShieldBlock")},
        .Rolls = {{20, 10}, {6, 6}},
        .Changes = {{"Actor 1.hitpoints.current", 96}, {"Actor 1.shield.hitpoints", 16}, {"Actor 1.reactions.count", 0},
                    {"Actor 0.actions.count", 9}, {"Actor 0.attacks_made", 1}}});
    EXPECT_TRUE(HasRecord());
    EXPECT_EQ(Options("trip.UseKnockdown").size(), 1U);
}

}
