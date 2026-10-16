#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/lifecycle_execution.h>
#include <pf2e_engine/actions/reaction_window.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/inventory/shield_actions.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

class TGeneratedShieldTest : public TBattleFixture {
protected:
    void SetUp() override
    {
        shield_rules_ = std::make_shared<TAbilityRules>();
        RegisterShieldActionsRules(*shield_rules_);
        StartBattle(shield_rules_, 6);
    }

    static TShield Shield(bool raised = true, int hitpoints = 20, bool wielded = true)
    {
        return TShield::Create({.Name = "Steel shield", .ArmorClassBonus = 2,
            .Hardness = 5, .MaximumHitpoints = 20, .Hitpoints = hitpoints,
            .BrokenThreshold = 10, .Wielded = wielded, .Raised = raised});
    }

    TPlayer& Add(TPosition position, bool shield = false, bool raised = true,
                 int current = 100, int temporary = 0, int resistance = 0,
                 EDamageType type = EDamageType::Slashing,
                 EZeroHpRule health = EZeroHpRule::Defeat, int shield_hitpoints = 20)
    {
        const int id = static_cast<int>(creatures_.size());
        TCreature::TInitialValues initial;
        initial.Name = "Actor " + std::to_string(id);
        initial.Hitpoints = THitPoints::Create({
            .Maximum = Statistic::Create({.Base = 100}), .Current = current,
            .Temporary = temporary,
        });
        initial.ArmorClassBaseline = TStatisticBaseline::Create({.IsFixed = true, .Value = 15});
        TIdCollection<TWeapon> weapons;
        weapons.Add(TWeapon::Create({.Name = "Blade", .BaseDiceSize = 12,
            .AttackBonusBaseline = TStatisticBaseline::Create({.IsFixed = true, .Value = 10}),
            .DamageModifierBaseline = TStatisticBaseline::Create({.IsFixed = true, .Value = 0}),
            .DamageType = type, .Reach = 1,
            .Features = Features({"abilities.WeaponStrike"})}));
        initial.Weapons = std::move(weapons);
        if (shield) {
            initial.Shield = Shield(raised, shield_hitpoints);
            initial.Features = Features({"shield_actions.ShieldActions", "shield_actions.ShieldBlockTraining"});
        }
        if (resistance != 0) {
            initial.Resistances = {{EDamageType::Slashing, resistance}};
        }
        initial.ZeroHpRule = health;
        initial.Actions = TResource(3);
        initial.Reactions = TResource(1);
        return Join(TCreature::Create(initial), id, position);
    }

    void Strike(int damage = 9, int natural = 10, bool suspend = false)
    {
        io_.ExpectName(0, "target", "Actor 1", suspend);
        rng_.ExpectCall(20, natural);
        if (damage != 0) {
            rng_.ExpectCall(12, damage);
        }
    }

    void Block(bool suspend = false)
    {
        io_.ExpectName(1, "reaction", "UseShieldBlock", suspend);
    }

    std::shared_ptr<TAbilityRules> shield_rules_;
};

TEST_F(TGeneratedShieldTest, DeclaredSteelShieldDefaultsAndNestedSnapshotsRoundTrip)
{
    TGameObjectFactory factory;
    const auto shield = CreateSteelShield();
    EXPECT_EQ(shield.Name(), "Steel shield");
    EXPECT_EQ(shield.Hitpoints(), 20);
    EXPECT_EQ(shield.MaximumHitpoints(), 20);
    EXPECT_TRUE(shield.CanRaise());
    EXPECT_FALSE(shield.IsRaised());
    const auto creature = TCreature::Create({.Name = "Shield bearer", .Shield = shield});
    const auto json = creature.ToJson();
    const auto restored = TCreature::Restore(json, factory);
    EXPECT_EQ(restored.ToJson(), json);
    TAstContext first_context;
    TAstContext second_context;
    EXPECT_EQ(creature.GetAst(first_context), restored.GetAst(second_context));
    const auto loaded = TCreature::FromJson({{"name", "Shield bearer"}, {"shield", shield.ToJson()}}, factory);
    EXPECT_EQ(loaded.Shield().ToJson(), shield.ToJson());
}

TEST_F(TGeneratedShieldTest, InvalidShieldAndInvalidDamageRejectBlockingBeforeMutation)
{
    Add({0, 0}, true);
    auto execution = journal_->ExecutionContext();
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    EXPECT_THROW(execution.Invoke(Creature(0).Shield(), &TShield::Block, 0), std::runtime_error);
    EXPECT_EQ(Snapshot(), before);
    execution.AssignGuarded(Creature(0).Shield(), TShield::Create({
        .Hardness = -1, .MaximumHitpoints = 20, .Hitpoints = 20,
        .BrokenThreshold = 10, .Wielded = true, .Raised = true,
    }), TGuarded<TCreature>(Creature(0)), "shield");
    EXPECT_FALSE(Read(0).Shield().IsValid());
    EXPECT_FALSE(Read(0).Shield().CanRaise());
    const auto invalid = Snapshot();
    EXPECT_THROW(execution.Invoke(Creature(0).Shield(), &TShield::Block, 9), std::runtime_error);
    EXPECT_EQ(Snapshot(), invalid);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedShieldTest, RaiseShieldPaysOneActionAndMergesCircumstanceBonuses)
{
    Add({0, 0}, true, false);
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    EXPECT_EQ(Read(0).ArmorClass(), 15);
    EXPECT_EQ(Execute("shield_actions.RaiseShield"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    EXPECT_TRUE(Read(0).Shield().IsRaised());
    EXPECT_EQ(Read(0).ArmorClass(), 17);
    EXPECT_EQ(Read(0).ArmorClassWithModifiers(TCheckModifiers::Create({
        .StatusBonus = 1, .CircumstanceBonus = 3, .CircumstancePenalty = 1,
    })), 18);
    EXPECT_EQ(io_.Requests(), 0U);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedShieldTest, MissingBrokenAndUnwieldedShieldsCannotRaise)
{
    Add({0, 0}, true, false, 100, 0, 0, EDamageType::Slashing, EZeroHpRule::Defeat, 10);
    EXPECT_FALSE(Available("shield_actions.RaiseShield"));
    auto execution = journal_->ExecutionContext();
    execution.AssignGuarded(Creature(0).Shield(), Shield(false, 20, false),
        TGuarded<TCreature>(Creature(0)), "shield");
    EXPECT_FALSE(Available("shield_actions.RaiseShield"));
    execution.AssignGuarded(Creature(0).Shield(), TShield::Create({}),
        TGuarded<TCreature>(Creature(0)), "shield");
    EXPECT_FALSE(Available("shield_actions.RaiseShield"));
    EXPECT_EQ(Read(0).Actions().Count(), 3);
    EXPECT_EQ(io_.Requests(), 0U);
}

TEST_F(TGeneratedShieldTest, ArmorAndShieldMergeTheirOwnBonusTypesWithExplicitModifiers)
{
    const auto creature = TCreature::Create({
        .Level = 1,
        .Characteristic = TAbilityScores::Create({
            .Dexterity = TAbilityScore::Create({.Value = 14}),
        }),
        .Proficiency = TProficiency::Create({.ArmorCategory = std::map<EArmorCategory, TProficiencyValue>{{
            EArmorCategory::Light, TProficiencyValue::Create({.Rank = EProficiencyRank::Trained}),
        }}}),
        .Armor = TArmor::Create({.Category = EArmorCategory::Light,
            .ArmorClassBonus = 2, .DexterityCap = 3}),
        .Shield = Shield(),
    });
    EXPECT_EQ(creature.ArmorClass(), 19);
    EXPECT_EQ(creature.ArmorClassWithModifiers(TCheckModifiers::Create({
        .ItemBonus = 3, .CircumstanceBonus = 1,
    })), 20);
    EXPECT_EQ(creature.ArmorClassWithModifiers(TCheckModifiers::Create({
        .ItemBonus = 1, .StatusPenalty = 2, .CircumstanceBonus = 3,
    })), 18);
}

TEST_F(TGeneratedShieldTest, StrikeResolvesResistanceThenHardnessThenTemporaryHitpointsExactlyOnce)
{
    Add({0, 0});
    Add({1, 0}, true, true, 100, 1, 2);
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    Strike();
    io_.ExpectName(1, "reaction", "UseShieldBlock", false, [&](const TAlternatives&) {
        EXPECT_EQ(Read(0).Actions().Count(), 2);
        EXPECT_EQ(Read(0).AttacksMade(), 1);
        EXPECT_EQ(Read(1).Shield().Hitpoints(), 20);
        EXPECT_EQ(Read(1).Hitpoints().Current(), 100);
        EXPECT_EQ(Read(1).Hitpoints().Temporary(), 1);
    });
    EXPECT_EQ(Execute("abilities.Strike"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Shield().Hitpoints(), 18);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 99);
    EXPECT_EQ(Read(1).Hitpoints().Temporary(), 0);
    EXPECT_EQ(Read(1).Reactions().Count(), 0);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedShieldTest, HardnessCanPreventAllDamageWithoutDamagingTheShield)
{
    Add({0, 0});
    Add({1, 0}, true);
    Strike(4);
    Block();
    EXPECT_EQ(Execute("abilities.Strike"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 100);
    EXPECT_EQ(Read(1).Shield().Hitpoints(), 20);
    EXPECT_EQ(Read(1).Reactions().Count(), 0);
}

TEST_F(TGeneratedShieldTest, DeclinePreservesTheShieldAndPaysFullResolvedCreatureDamage)
{
    Add({0, 0});
    Add({1, 0}, true, true, 100, 1, 2);
    Strike();
    io_.ExpectName(1, "reaction", "Decline");
    EXPECT_EQ(Execute("abilities.Strike"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 94);
    EXPECT_EQ(Read(1).Hitpoints().Temporary(), 0);
    EXPECT_EQ(Read(1).Shield().Hitpoints(), 20);
    EXPECT_EQ(Read(1).Reactions().Count(), 1);
}

TEST_F(TGeneratedShieldTest, BreakingTheShieldRemovesItsArmorBonusAndFutureBlockEligibility)
{
    Add({0, 0});
    Add({1, 0}, true, true, 100, 0, 0, EDamageType::Slashing, EZeroHpRule::Defeat, 12);
    Strike(9);
    Block();
    EXPECT_EQ(Execute("abilities.Strike"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Shield().Hitpoints(), 8);
    EXPECT_TRUE(Read(1).Shield().IsBroken());
    EXPECT_FALSE(Read(1).Shield().IsRaised());
    EXPECT_EQ(Read(1).ArmorClass(), 15);
    EXPECT_FALSE(Available("shield_actions.RaiseShield", 1));
    auto execution = journal_->ExecutionContext();
    execution.Invoke(TGuarded<TCreature>(Creature(1)), &TCreature::BeginTurn);
    Strike(3);
    EXPECT_EQ(Execute("abilities.Strike"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Shield().Hitpoints(), 8);
    EXPECT_EQ(Read(1).Reactions().Count(), 1);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 93);
}

TEST_F(TGeneratedShieldTest, AShieldCanBeDestroyedByACriticalStrikeWhilePassingTheRemainder)
{
    Add({0, 0});
    Add({1, 0}, true, true, 100, 0, 0, EDamageType::Slashing, EZeroHpRule::Defeat, 15);
    Strike(12, 20);
    Block();
    EXPECT_EQ(Execute("abilities.Strike"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Shield().Hitpoints(), 0);
    EXPECT_TRUE(Read(1).Shield().IsDestroyed());
    EXPECT_TRUE(Read(1).Shield().IsBroken());
    EXPECT_EQ(Read(1).Hitpoints().Current(), 81);
}

TEST_F(TGeneratedShieldTest, NonphysicalAttackDoesNotOfferShieldBlock)
{
    Add({0, 0}, false, true, 100, 0, 0, EDamageType::Fire);
    Add({1, 0}, true);
    Strike();
    EXPECT_EQ(Execute("abilities.Strike"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 91);
    EXPECT_EQ(Read(1).Shield().Hitpoints(), 20);
    EXPECT_EQ(Read(1).Reactions().Count(), 1);
}

TEST_F(TGeneratedShieldTest, ResistanceReducingTheAttackToZeroDoesNotSpendAReaction)
{
    Add({0, 0});
    Add({1, 0}, true, true, 100, 0, 20);
    Strike();
    EXPECT_EQ(Execute("abilities.Strike"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 100);
    EXPECT_EQ(Read(1).Shield().Hitpoints(), 20);
    EXPECT_EQ(Read(1).Reactions().Count(), 1);
}

TEST_F(TGeneratedShieldTest, RaisedArmorClassCanTurnTheStrikeIntoAMissBeforeTheWindow)
{
    Add({0, 0});
    Add({1, 0}, true);
    Strike(0, 5);
    EXPECT_EQ(Execute("abilities.Strike"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 100);
    EXPECT_EQ(Read(1).Reactions().Count(), 1);
    EXPECT_EQ(Read(0).AttacksMade(), 1);
}

TEST_F(TGeneratedShieldTest, OnlyTheAttackedShieldBearerReceivesTheBlockOffer)
{
    Add({0, 0});
    Add({1, 0}, true);
    Add({0, 1}, true);
    Strike();
    Block();
    EXPECT_EQ(Execute("abilities.Strike"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 96);
    EXPECT_EQ(Read(2).Hitpoints().Current(), 100);
    EXPECT_EQ(Read(2).Reactions().Count(), 1);
}

TEST_F(TGeneratedShieldTest, SuspendedBlockRechecksRaisedStateBeforeItsOwnPaymentAndReplaysPendingDamage)
{
    Add({0, 0});
    Add({1, 0}, true);
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    Strike();
    Block(true);
    const auto point = Pause([&] { Execute("abilities.Strike"); });
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    EXPECT_EQ(Read(1).Reactions().Count(), 1);
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    auto execution = editor.ExecutionContext();
    execution.Invoke(Creature(1).Shield(), &TShield::Lower);
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Read(1).Reactions().Count(), 1);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 100);
    editor.Undo(edit);
    point.Resume();
    EXPECT_EQ(Read(1).Shield().Hitpoints(), 16);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 96);
    const auto completed = Snapshot();
    point.Revert(*journal_);
    point.Resume();
    EXPECT_EQ(Snapshot(), completed);
    EXPECT_EQ(rng_.RemainingCalls(), 0U);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedShieldTest, ShieldBlockCompletesBeforeTheRemainderKnocksOutAndDropsTheShield)
{
    Add({0, 0});
    Add({1, 0}, true, true, 3, 0, 0, EDamageType::Slashing, EZeroHpRule::Dying);
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    Strike();
    Block(true);
    const auto point = Pause([&] { Execute("abilities.Strike"); });
    point.Resume();
    EXPECT_EQ(Read(1).Shield().Hitpoints(), 16);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 0);
    EXPECT_EQ(Read(1).DyingValue(), 1);
    EXPECT_TRUE(Read(1).Conditions().Has(EConditionKind::Unconscious));
    EXPECT_FALSE(Read(1).Shield().Wielded());
    EXPECT_FALSE(Read(1).Shield().IsRaised());
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    EXPECT_EQ(Read(0).AttacksMade(), 1);
    const auto completed = Snapshot();
    point.Revert(*journal_);
    point.Resume();
    EXPECT_EQ(Snapshot(), completed);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedShieldTest, ExpirationLowersOnlyTheCurrentActorsShieldAndIsJournaled)
{
    Add({0, 0}, true);
    Add({1, 0}, true);
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    auto execution = journal_->ExecutionContext();
    execution.Invoke(TGuarded<TCreature>(Creature(0)), &TCreature::ExpireTurnStartEffects);
    EXPECT_FALSE(Read(0).Shield().IsRaised());
    EXPECT_TRUE(Read(1).Shield().IsRaised());
    EXPECT_EQ(Read(0).ArmorClass(), 15);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedShieldTest, TurnStartExpirationPrecedesScheduledEffectsAndResourceReplenishment)
{
    Add({0, 0}, true);
    Add({1, 0}, true);
    int scheduled_calls = 0;
    struct TReachedScheduledCheck : std::exception {};
    shield_rules_->Lifecycle.Register(ELifecyclePhase::RoundStart,
        std::make_shared<const TLifecycleExecution>(TLifecycleExecution{
            .OwnerType = typeid(TCreature),
            .Run = [&](std::shared_ptr<TActionContext> context, std::function<void()>) {
                const auto actor = ActionSelf<TCreature>(context);
                auto execution = context->transformator->ExecutionContext();
                execution.Invoke(actor.Guard(), &TCreature::BeginRound);
                if (actor.Read().Id() != Read(0).Id()) {
                    return;
                }
                execution.SetResourceCount(Creature(0).Actions(), 0, actor.Guard(), "actions");
                context->transformator->AddTask(context->scheduler, {
                    .events_before_call = {{.type = EEvent::OnTurnStart, .context = {.player = &Player(0)}}},
                    .callback = [&]() -> bool {
                        ++scheduled_calls;
                        EXPECT_FALSE(Read(0).Shield().IsRaised());
                        EXPECT_TRUE(Read(1).Shield().IsRaised());
                        EXPECT_EQ(Read(0).Actions().Count(), 0);
                        EXPECT_EQ(Read(0).ArmorClass(), 15);
                        throw TReachedScheduledCheck{};
                    },
                });
            },
        }));
    EXPECT_THROW(battle_->StartBattle(), TReachedScheduledCheck);
    EXPECT_EQ(scheduled_calls, 1);
    EXPECT_EQ(io_.Requests(), 0U);
}

}
