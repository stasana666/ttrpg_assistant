#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/inventory/movement.h>
#include <pf2e_engine/inventory/shield_actions.h>
#include <pf2e_engine/inventory/trip.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

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

    static TWeapon Weapon(bool knockdown = true, bool strike = true,
                          TVariantMap<EWeaponTraitKind, TWeaponTrait> traits = {})
    {
        auto features = Features({});
        if (knockdown) {
            features.Add(TFeatureSelection("trip.Knockdown"));
        }
        if (strike) {
            features.Add(TFeatureSelection("abilities.WeaponStrike"));
        }
        return TWeapon::Create({.Name = "Jaws", .BaseDiceSize = 6,
            .AttackBonusBaseline = TStatisticBaseline::Create({.IsFixed = true, .Value = 10}),
            .DamageModifierBaseline = TStatisticBaseline::Create({.IsFixed = true, .Value = 0}),
            .DamageType = EDamageType::Piercing, .Category = EWeaponCategory::Unarmed,
            .Reach = 1, .HandsRequired = 0, .Traits = std::move(traits),
            .Features = std::move(features)});
    }

    static TIdCollection<TWeapon> Weapons(std::initializer_list<TWeapon> weapons)
    {
        TIdCollection<TWeapon> result;
        for (const auto& weapon : weapons) {
            result.Add(weapon);
        }
        return result;
    }

    static TCreature::TInitialValues Profile()
    {
        TCreature::TInitialValues initial;
        initial.Size = ECreatureSize::Medium;
        initial.Movement = 5;
        initial.SaveBaselines = {{ESavethrow::Reflex,
            TStatisticBaseline::Create({.IsFixed = true, .Value = 5})}};
        initial.SkillBaselines = {{ESkill::Athletics,
            TStatisticBaseline::Create({.IsFixed = true, .Value = 10})}};
        initial.Hitpoints = THitPoints::Create({
            .Maximum = Statistic::Create({.Base = 100}), .Current = 100,
        });
        initial.ArmorClassBaseline = TStatisticBaseline::Create({.IsFixed = true, .Value = 15});
        initial.NaturalWeapons = Weapons({Weapon()});
        initial.Features = Features({"abilities.BasicStrike", "trip.TripActions", "movement.MovementActions"});
        initial.Actions = TResource(10);
        initial.Reactions = TResource(1);
        initial.Flourishes = TResource(1);
        initial.Hands = TResource(1);
        return initial;
    }

    TPlayer& Add(TPosition position, TCreature::TInitialValues initial = Profile())
    {
        const int id = static_cast<int>(creatures_.size());
        initial.Name = "Actor " + std::to_string(id);
        return Join(TCreature::Create(initial), id, position);
    }

    void Target(int target = 1, bool suspend = false, int actor = 0)
    {
        io_.ExpectName(actor, "target", "Actor " + std::to_string(target), suspend);
    }

    void Hit(int target = 1, int natural = 10, int damage = 2)
    {
        Target(target);
        rng_.ExpectCall(20, natural);
        rng_.ExpectCall(6, damage);
    }

    bool HasRecord(int actor = 0)
    {
        return battle_->History().Find(Player(actor), TRecordStrikeResult::kId).has_value();
    }

    static TGuarded<TWeapon> NaturalWeapon(TGeneratedExecutionContext& execution, TCreature& creature)
    {
        return execution.Project(creature.NaturalWeapons(), [](TIdCollection<TWeapon>& values) {
            return TGuarded<TWeapon>(*values.Find(TId<TWeapon>{0}));
        });
    }
};

TEST_F(TGeneratedTripTest, OrdinaryTripResolvesAllFourOutcomesAndRollsBack)
{
    Add({1, 1});
    Add({2, 1});
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    struct TCase {
        int Natural;
        bool TargetProne;
        bool ActorProne;
        int Damage;
    };
    for (const auto& test : std::vector<TCase>{{20, true, false, 6}, {5, true, false, 0},
                                             {2, false, false, 0}, {1, false, true, 0}}) {
        SCOPED_TRACE(test.Natural);
        Target();
        rng_.ExpectCall(20, test.Natural);
        if (test.Damage > 0) {
            rng_.ExpectCall(6, test.Damage);
        }
        EXPECT_EQ(Execute("trip.UseTrip"), EAbilityExecutionResult::Applied);
        EXPECT_EQ(Read(0).Actions().Count(), 9);
        EXPECT_EQ(Read(0).Hands().Count(), 1);
        EXPECT_EQ(Read(0).AttacksMade(), 1);
        EXPECT_EQ(Read(0).Conditions().Has(EConditionKind::Prone), test.ActorProne);
        EXPECT_EQ(Read(1).Conditions().Has(EConditionKind::Prone), test.TargetProne);
        EXPECT_EQ(Read(1).Hitpoints().Current(), 100 - test.Damage);
        EXPECT_FALSE(HasRecord());
        journal_->Undo(initial);
        EXPECT_EQ(Snapshot(), before);
    }
}

TEST_F(TGeneratedTripTest, FreeHandTripUsesNonAgilePenaltyAndTheSharedAttackCounter)
{
    auto actor = Profile();
    TVariantMap<EWeaponTraitKind, TWeaponTrait> traits;
    traits.Set(TWeaponTraitAgile{});
    actor.NaturalWeapons = Weapons({Weapon(true, true, traits)});
    Add({1, 1}, actor);
    Add({2, 1});
    Hit();
    Execute("abilities.Strike");
    ASSERT_EQ(Read(0).AttacksMade(), 1);
    Target();
    rng_.ExpectCall(20, 9);
    Execute("trip.UseTrip");
    EXPECT_EQ(Read(0).AttacksMade(), 2);
    EXPECT_FALSE(Read(1).Conditions().Has(EConditionKind::Prone));
    EXPECT_EQ(Read(1).Hitpoints().Current(), 98);
    EXPECT_EQ(Read(0).Actions().Count(), 8);
    EXPECT_FALSE(HasRecord());
}

TEST_F(TGeneratedTripTest, TripRejectsMissingFreeHandSizeAndBodyReachBeforePayment)
{
    auto actor = Profile();
    auto long_weapon = TWeapon::Create({.Name = "Long weapon", .BaseDiceSize = 6, .Reach = 3});
    actor.Weapons = Weapons({long_weapon});
    actor.Hands = TResource(0);
    Add({1, 1}, actor);
    Add({3, 1});
    const auto before = Snapshot();
    EXPECT_TRUE(Options("trip.UseTrip").empty());
    TTransformator editor(io_);
    const auto initial = editor.CurrentState();
    auto execution = editor.ExecutionContext();
    execution.SetResourceCount(Creature(0).Hands(), 1, TGuarded<TCreature>(Creature(0)), "hands");
    EXPECT_TRUE(Options("trip.UseTrip").empty());
    editor.MovePlayer(&Player(1), {2, 1});
    execution.AssignGuarded(Creature(1).Size(), ECreatureSize::Huge,
        TGuarded<TCreature>(Creature(1)), "size");
    EXPECT_TRUE(Options("trip.UseTrip").empty());
    execution.AssignGuarded(Creature(1).Size(), ECreatureSize::Large,
        TGuarded<TCreature>(Creature(1)), "size");
    EXPECT_EQ(Options("trip.UseTrip").size(), 1U);
    EXPECT_EQ(Read(0).Actions().Count(), 10);
    EXPECT_EQ(io_.Requests(), 0U);
    editor.Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedTripTest, TenFootBodyReachIncludesTheSecondDiagonal)
{
    auto actor = Profile();
    actor.BodyReach = 2;
    Add({1, 1}, actor);
    Add({3, 3});
    Target();
    rng_.ExpectCall(20, 5);
    EXPECT_EQ(Execute("trip.UseTrip"), EAbilityExecutionResult::Applied);
    EXPECT_TRUE(Read(1).Conditions().Has(EConditionKind::Prone));
    const auto rules = TTripRules::Create({});
    EXPECT_TRUE(rules.InReach(1, 1, 3, 3, 2));
    EXPECT_FALSE(rules.InReach(1, 1, 3, 3, 1));
    EXPECT_FALSE(rules.InReach(0, 0, 3, 3, 3));
}

TEST_F(TGeneratedTripTest, PausedTripRechecksTargetAndFreeHandBeforeRolling)
{
    Add({1, 1});
    Add({2, 1});
    Target(1, true);
    const auto point = Pause([&] { Execute("trip.UseTrip"); });
    EXPECT_EQ(Read(0).Actions().Count(), 9);
    TTransformator editor(io_);
    const auto start = editor.CurrentState();
    editor.MovePlayer(&Player(1), {4, 1});
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    editor.Undo(start);
    editor.ExecutionContext().SetResourceCount(Creature(0).Hands(), 0,
        TGuarded<TCreature>(Creature(0)), "hands");
    EXPECT_THROW(point.Resume(), std::runtime_error);
    EXPECT_EQ(Read(0).AttacksMade(), 0);
    editor.Undo(start);
    rng_.ExpectCall(20, 5);
    point.Resume();
    EXPECT_TRUE(Read(1).Conditions().Has(EConditionKind::Prone));
    EXPECT_EQ(Read(0).Actions().Count(), 9);
}

TEST_F(TGeneratedTripTest, CriticalTripUsesAttackDefensesShieldHardnessAndTemporaryHp)
{
    Add({1, 1});
    auto target = Profile();
    target.Hitpoints = THitPoints::Create({
        .Maximum = Statistic::Create({.Base = 100}), .Current = 100, .Temporary = 2});
    target.Resistances = {{EDamageType::Bludgeoning, 1}};
    target.Shield = TShield::Create({.Name = "Shield", .ArmorClassBonus = 2, .Hardness = 2,
        .MaximumHitpoints = 20, .BrokenThreshold = 10, .Wielded = true, .Raised = true});
    target.Features = Features({"shield_actions.ShieldBlockTraining"});
    Add({2, 1}, target);
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    Target();
    rng_.ExpectCall(20, 20);
    rng_.ExpectCall(6, 6);
    io_.ExpectName(1, "reaction", "UseShieldBlock", true);
    const auto point = Pause([&] { Execute("trip.UseTrip"); });
    EXPECT_TRUE(Read(1).Conditions().Has(EConditionKind::Prone));
    EXPECT_EQ(Read(1).Hitpoints().Current(), 100);
    point.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 99);
    EXPECT_EQ(Read(1).Hitpoints().Temporary(), 0);
    EXPECT_EQ(Read(1).Shield().Hitpoints(), 17);
    EXPECT_EQ(Read(1).Reactions().Count(), 0);
    EXPECT_EQ(Read(0).AttacksMade(), 1);
    const auto after = Snapshot();
    point.Revert(*journal_);
    point.Resume();
    EXPECT_EQ(Snapshot(), after);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedTripTest, KnockdownFollowsTheSameJawsWithoutFreeHandMapOrExtraAttackCount)
{
    auto actor = Profile();
    actor.Hands = TResource(0);
    actor.AttacksMade = 2;
    Add({1, 1}, actor);
    Add({2, 1});
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
    EXPECT_TRUE(Options("trip.UseTrip").empty());
    Hit(1, 15);
    Execute("abilities.Strike");
    ASSERT_EQ(Options("trip.UseKnockdown").size(), 1U);
    EXPECT_EQ(Read(0).AttacksMade(), 3);
    rng_.ExpectCall(20, 5);
    Execute("trip.UseKnockdown");
    EXPECT_EQ(Read(0).AttacksMade(), 3);
    EXPECT_EQ(Read(0).Actions().Count(), 8);
    EXPECT_EQ(Read(0).Hands().Count(), 0);
    EXPECT_EQ(io_.Requests(), 1U);
    EXPECT_TRUE(Read(1).Conditions().Has(EConditionKind::Prone));
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedTripTest, ActorStrikeSelectsAnUngrantedWeaponAndPublishesItsExactIdentity)
{
    auto actor = Profile();
    actor.NaturalWeapons = Weapons({Weapon(true, false), Weapon(true, false)});
    Add({1, 1}, actor);
    Add({2, 1});
    EXPECT_TRUE(Options("abilities.Strike").empty());
    Target();
    io_.ExpectName(0, "source", "Jaws (2)");
    rng_.ExpectCall(20, 10);
    rng_.ExpectCall(6, 2);
    Execute("abilities.UseStrike");
    const auto options = Options("trip.UseKnockdown");
    ASSERT_EQ(options.size(), 1U);
    EXPECT_TRUE(options[0].Source.StartsWith(TAbilitySourcePath{}.Field("natural_weapons").Entry(1)));
    rng_.ExpectCall(20, 5);
    Execute(options[0]);
    EXPECT_TRUE(Read(1).Conditions().Has(EConditionKind::Prone));
    EXPECT_EQ(Read(0).AttacksMade(), 1);
}

TEST_F(TGeneratedTripTest, MissCannotQualifyAndFailedKnockdownStillConsumesItsHistory)
{
    Add({1, 1});
    Add({2, 1});
    Target();
    rng_.ExpectCall(20, 2);
    Execute("abilities.Strike");
    EXPECT_TRUE(HasRecord());
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
    Hit(1, 10);
    Execute("abilities.Strike");
    ASSERT_EQ(Options("trip.UseKnockdown").size(), 1U);
    rng_.ExpectCall(20, 1);
    Execute("trip.UseKnockdown");
    EXPECT_TRUE(Read(0).Conditions().Has(EConditionKind::Prone));
    EXPECT_EQ(Read(0).AttacksMade(), 2);
    EXPECT_EQ(Read(0).Actions().Count(), 7);
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
}

TEST_F(TGeneratedTripTest, AddingKnockdownAfterTheStrikeDoesNotChangeTheHistoricalDamageEntry)
{
    auto actor = Profile();
    actor.NaturalWeapons = Weapons({Weapon(false)});
    Add({1, 1}, actor);
    Add({2, 1});
    Hit();
    Execute("abilities.Strike");
    TTransformator editor(io_);
    auto execution = editor.ExecutionContext();
    const auto weapon = NaturalWeapon(execution, Creature(0));
    const auto features = execution.Project(weapon, [](TWeapon& value) { return value.Features(); });
    execution.AssignGuarded(features, Features({"abilities.WeaponStrike", "trip.Knockdown"}),
        weapon, "features");
    EXPECT_TRUE(HasRecord());
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
    Hit(1, 10);
    Execute("abilities.Strike");
    EXPECT_EQ(Options("trip.UseKnockdown").size(), 1U);
}

TEST_F(TGeneratedTripTest, NewStrikeInvalidatesSavedKnockdownRatherThanChangingItsTarget)
{
    Add({1, 1});
    Add({2, 1});
    Add({1, 2});
    Hit();
    Execute("abilities.Strike");
    const auto stale = Option("trip.UseKnockdown");
    Hit(2, 10);
    Execute("abilities.Strike");
    const auto before = Snapshot();
    EXPECT_EQ(Execute(stale), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(Snapshot(), before);
    rng_.ExpectCall(20, 5);
    Execute("trip.UseKnockdown");
    EXPECT_FALSE(Read(1).Conditions().Has(EConditionKind::Prone));
    EXPECT_TRUE(Read(2).Conditions().Has(EConditionKind::Prone));
}

TEST_F(TGeneratedTripTest, SourceReplacementAndLiveRangeCannotSatisfyASavedKnockdown)
{
    Add({1, 1});
    Add({2, 1});
    Hit();
    Execute("abilities.Strike");
    const auto option = Option("trip.UseKnockdown");
    TTransformator editor(io_);
    const auto initial = editor.CurrentState();
    editor.MovePlayer(&Player(1), {4, 1});
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
    EXPECT_EQ(Execute(option), EAbilityExecutionResult::Unavailable);
    editor.Undo(initial);
    auto weapons = Read(0).NaturalWeapons();
    weapons.Remove(TId<TWeapon>{0});
    weapons.Add(Weapon());
    editor.ExecutionContext().AssignGuarded(Creature(0).NaturalWeapons(), std::move(weapons),
        TGuarded<TCreature>(Creature(0)), "natural_weapons");
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
    EXPECT_EQ(Execute(option), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(Read(0).Actions().Count(), 9);
    editor.Undo(initial);
    EXPECT_EQ(Options("trip.UseKnockdown").size(), 1U);
}

TEST_F(TGeneratedTripTest, AnotherActorsActionPreservesTheResultButOwnStepInvalidatesIt)
{
    Add({1, 1});
    Add({2, 1});
    Hit();
    Execute("abilities.Strike");
    io_.ExpectValue(1, TPosition{3, 1});
    Execute("movement.UseStep", 1);
    EXPECT_TRUE(HasRecord());
    io_.ExpectValue(1, TPosition{2, 1});
    Execute("movement.UseStep", 1);
    EXPECT_EQ(Options("trip.UseKnockdown").size(), 1U);
    io_.ExpectValue(0, TPosition{1, 2});
    Execute("movement.UseStep");
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
}

TEST_F(TGeneratedTripTest, ViciousSwingSubordinateStrikeDoesNotPublishAStandaloneStrikeResult)
{
    auto actor = Profile();
    actor.Features = Features({"abilities.BasicStrike", "abilities.ViciousSwing"});
    Add({1, 1}, actor);
    Add({2, 1});
    Target();
    io_.ExpectName(0, "source", "Jaws");
    rng_.ExpectCall(20, 10);
    rng_.ExpectCall(6, 2);
    rng_.ExpectCall(6, 2);
    Execute("abilities.UseViciousSwing");
    EXPECT_EQ(Read(0).Actions().Count(), 8);
    EXPECT_EQ(Read(0).AttacksMade(), 2);
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("trip.UseKnockdown").empty());
}

TEST_F(TGeneratedTripTest, VersatileSelectsBaseOrAlternativeBeforeTheAttackRoll)
{
    auto actor = Profile();
    TVariantMap<EWeaponTraitKind, TWeaponTrait> traits;
    traits.Set(TWeaponTraitVersatile{EDamageType::Slashing});
    actor.NaturalWeapons = Weapons({Weapon(false, true, traits)});
    Add({1, 1}, actor);
    auto target = Profile();
    target.Immunities = {EDamageType::Piercing};
    target.Vulnerabilities = {{EDamageType::Slashing, 3}};
    Add({2, 1}, target);
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    for (const auto type : {EDamageType::Piercing, EDamageType::Slashing}) {
        Target();
        io_.ExpectValue(0, type, false, [&](const TAlternatives& alternatives) {
            EXPECT_EQ(alternatives.Size(), 2U);
            EXPECT_EQ(rng_.RemainingCalls(), 2U);
            EXPECT_EQ(Read(0).AttacksMade(), 0);
        });
        rng_.ExpectCall(20, 10);
        rng_.ExpectCall(6, 4);
        Execute("abilities.Strike");
        EXPECT_EQ(Read(1).Hitpoints().Current(), type == EDamageType::Piercing ? 100 : 93);
        EXPECT_EQ(Read(0).Actions().Count(), 9);
        EXPECT_EQ(Read(0).AttacksMade(), 1);
        EXPECT_TRUE(HasRecord());
        journal_->Undo(initial);
        EXPECT_EQ(Snapshot(), before);
    }
}

TEST_F(TGeneratedTripTest, PausedVersatileChoiceRechecksTheTargetAndRetainsSourceValues)
{
    auto actor = Profile();
    TVariantMap<EWeaponTraitKind, TWeaponTrait> traits;
    traits.Set(TWeaponTraitVersatile{EDamageType::Slashing});
    actor.NaturalWeapons = Weapons({Weapon(true, true, traits)});
    Add({1, 1}, actor);
    auto target = Profile();
    target.Immunities = {EDamageType::Piercing};
    Add({2, 1}, target);
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    Target();
    io_.ExpectValue(0, EDamageType::Slashing, true);
    const auto point = Pause([&] { Execute("abilities.Strike"); });
    EXPECT_FALSE(HasRecord());
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    editor.MovePlayer(&Player(1), {4, 1});
    EXPECT_THROW(point.Resume(), std::runtime_error);
    EXPECT_EQ(Read(0).AttacksMade(), 0);
    editor.Undo(edit);
    auto execution = editor.ExecutionContext();
    const auto weapon = NaturalWeapon(execution, Creature(0));
    const auto current_traits = execution.Project(weapon, [](TWeapon& value) { return value.Traits(); });
    execution.AssignGuarded(current_traits, TVariantMap<EWeaponTraitKind, TWeaponTrait>{}, weapon, "traits");
    rng_.ExpectCall(20, 10);
    rng_.ExpectCall(6, 4);
    point.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 96);
    EXPECT_TRUE(HasRecord());
    EXPECT_EQ(Read(0).Actions().Count(), 9);
    editor.Undo(edit);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedTripTest, VersatilePendingDamageAndPublicationReplayWithoutRerolling)
{
    auto actor = Profile();
    TVariantMap<EWeaponTraitKind, TWeaponTrait> traits;
    traits.Set(TWeaponTraitVersatile{EDamageType::Slashing});
    actor.NaturalWeapons = Weapons({Weapon(true, true, traits)});
    Add({1, 1}, actor);
    auto target = Profile();
    target.Immunities = {EDamageType::Piercing};
    target.Shield = TShield::Create({.Name = "Shield", .ArmorClassBonus = 2, .Hardness = 2,
        .MaximumHitpoints = 20, .BrokenThreshold = 10, .Wielded = true, .Raised = true});
    target.Features = Features({"shield_actions.ShieldBlockTraining"});
    Add({2, 1}, target);
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    Target();
    io_.ExpectValue(0, EDamageType::Slashing, true);
    const auto first = Pause([&] { Execute("abilities.Strike"); });
    rng_.ExpectCall(20, 10);
    rng_.ExpectCall(6, 6);
    io_.ExpectName(1, "reaction", "UseShieldBlock", true);
    const auto second = Pause([&] { first.Resume(); });
    EXPECT_FALSE(HasRecord());
    second.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 96);
    EXPECT_EQ(Read(1).Shield().Hitpoints(), 16);
    EXPECT_TRUE(HasRecord());
    EXPECT_EQ(Options("trip.UseKnockdown").size(), 1U);
    const auto completed = Snapshot();
    second.Revert(*journal_);
    second.Resume();
    EXPECT_EQ(Snapshot(), completed);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedTripTest, RedundantVersatileTypeDoesNotCreateAnExtraPrompt)
{
    auto actor = Profile();
    TVariantMap<EWeaponTraitKind, TWeaponTrait> traits;
    traits.Set(TWeaponTraitVersatile{EDamageType::Piercing});
    actor.NaturalWeapons = Weapons({Weapon(true, true, traits)});
    Add({1, 1}, actor);
    Add({2, 1});
    Hit();
    Execute("abilities.Strike");
    EXPECT_EQ(io_.Requests(), 1U);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 98);
}

TEST_F(TGeneratedTripTest, CriticalTripCarriesCriticalKnockoutContextWithoutDoublingItsDie)
{
    Add({1, 1});
    auto target = Profile();
    target.Hitpoints = THitPoints::Create({
        .Maximum = Statistic::Create({.Base = 100}), .Current = 3});
    target.ZeroHpRule = EZeroHpRule::Dying;
    Add({2, 1}, target);
    Target();
    rng_.ExpectCall(20, 20);
    rng_.ExpectCall(6, 3);
    Execute("trip.UseTrip");
    EXPECT_EQ(Read(1).Hitpoints().Current(), 0);
    EXPECT_EQ(Read(1).DyingValue(), 2);
    EXPECT_FALSE(Read(1).Dead());
    EXPECT_TRUE(Read(1).Conditions().Has(EConditionKind::Prone));
    EXPECT_TRUE(Read(1).Conditions().Has(EConditionKind::Unconscious));
}

TEST_F(TGeneratedTripTest, ConsumedKnockdownInputsSurviveShieldSuspensionAndSourceRestore)
{
    auto actor = Profile();
    actor.Hands = TResource(0);
    Add({1, 1}, actor);
    auto target = Profile();
    target.Shield = TShield::Create({.Name = "Shield", .ArmorClassBonus = 2, .Hardness = 2,
        .MaximumHitpoints = 20, .BrokenThreshold = 10, .Wielded = true});
    target.Features = Features({"shield_actions.ShieldBlockTraining"});
    Add({2, 1}, target);
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    Hit();
    Execute("abilities.Strike");
    journal_->ExecutionContext().Invoke(Creature(1).Shield(), &TShield::Raise);
    rng_.ExpectCall(20, 20);
    rng_.ExpectCall(6, 6);
    io_.ExpectName(1, "reaction", "UseShieldBlock", true);
    const auto point = Pause([&] { Execute("trip.UseKnockdown"); });
    EXPECT_FALSE(HasRecord());
    EXPECT_EQ(Read(0).Actions().Count(), 8);
    EXPECT_EQ(Read(0).AttacksMade(), 1);
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    editor.ExecutionContext().AssignGuarded(Creature(0).NaturalWeapons(), TIdCollection<TWeapon>{},
        TGuarded<TCreature>(Creature(0)), "natural_weapons");
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Read(1).Reactions().Count(), 0);
    EXPECT_EQ(Read(1).Shield().Hitpoints(), 16);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 98);
    editor.Undo(edit);
    point.Revert(*journal_);
    point.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 94);
    EXPECT_EQ(Read(1).Shield().Hitpoints(), 16);
    EXPECT_EQ(Read(0).AttacksMade(), 1);
    EXPECT_FALSE(HasRecord());
    const auto completed = Snapshot();
    point.Revert(*journal_);
    point.Resume();
    EXPECT_EQ(Snapshot(), completed);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

}
