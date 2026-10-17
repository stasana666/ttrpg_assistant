#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/lifecycle_execution.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/rules.h>
#include <pf2e_engine/inventory/remaster_profiles.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

class TGeneratedEncounterTest : public TBattleFixture {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>(*CreateAbilityRules());
        ASSERT_FALSE(rules->Lifecycle.Find(ELifecyclePhase::RoundStart));
        rules->Lifecycle.Register(ELifecyclePhase::RoundStart,
            std::make_shared<const TLifecycleExecution>(TLifecycleExecution{
                .OwnerType = typeid(TCreature),
                .Run = [this](std::shared_ptr<TActionContext> context, std::function<void()>) {
                    battle_journal_ = context->transformator;
                    battle_journal_->ExecutionContext().Invoke(ActionSelf<TCreature>(context).Guard(), &TCreature::BeginRound);
                },
            }));
        StartBattle(std::move(rules), 8);
        io_.ObserveLog([this] {
            if (battle_journal_ != nullptr && !checkpoint_ && battle_->CurrentTurnPlayer() == nullptr
                && battle_->ElapsedSeconds() == 6) {
                checkpoint_ = battle_journal_->CurrentState();
                checkpoint_ast_ = Snapshot();
            }
        });
    }

    void Action(int actor, const std::string& name, std::function<void()> observe = {})
    {
        io_.ExpectName(actor, "next action", name, false, [this, actor, observe = std::move(observe)](const TAlternatives&) {
            EXPECT_EQ(battle_->CurrentTurnPlayer(), &Player(actor));
            if (observe) {
                observe();
            }
        });
    }

    void Strike(int actor, const std::string& target, const std::string& weapon,
                int natural, int damage_size, int damage, bool versatile = false)
    {
        Action(actor, "UseStrike");
        io_.ExpectName(actor, "target", target);
        io_.ExpectName(actor, "source", weapon);
        if (versatile) {
            io_.ExpectValue(actor, EDamageType::Slashing);
        }
        rng_.ExpectCall(20, natural);
        if (damage_size != 0) {
            rng_.ExpectCall(damage_size, damage);
        }
    }

    void Knockdown(int actor, int natural)
    {
        Action(actor, "UseKnockdown", [this, actor] {
            EXPECT_EQ(Read(actor).AttacksMade(), 1);
            EXPECT_EQ(Read(actor).Actions().Count(), 2);
        });
        rng_.ExpectCall(20, natural);
    }

    TAstNode Snapshot()
    {
        TAstContext context;
        return battle_->GetAst(context);
    }

    void ReplayRemainingBattle(const std::function<void()>& script)
    {
        ASSERT_NE(battle_journal_, nullptr);
        ASSERT_TRUE(checkpoint_);
        ASSERT_TRUE(checkpoint_ast_);
        const auto completed = Snapshot();
        const auto winner = battle_->Winner();
        rng_.Verify();
        io_.Verify();
        battle_journal_->Undo(*checkpoint_);
        EXPECT_EQ(Snapshot(), *checkpoint_ast_);
        EXPECT_EQ(battle_->CurrentTurnPlayer(), nullptr);
        EXPECT_EQ(battle_->ElapsedSeconds(), 6U);
        script();
        battle_->StartBattle();
        EXPECT_EQ(battle_->Winner(), winner);
        EXPECT_EQ(Snapshot(), completed);
        rng_.Verify();
        io_.Verify();
        battle_journal_->Undo(*checkpoint_);
        EXPECT_EQ(Snapshot(), *checkpoint_ast_);
    }

    void FighterVictorySecondRound()
    {
        Action(0, "UseStand", [this] {
            EXPECT_EQ(Read(0).Actions().Count(), 3);
            EXPECT_EQ(Read(0).Reactions().Count(), 1);
            EXPECT_EQ(Read(0).Flourishes().Count(), 1);
            EXPECT_FALSE(Read(0).Shield().IsRaised());
            EXPECT_TRUE(Read(0).Conditions().Has(EConditionKind::Prone));
        });
        Action(0, "UseViciousSwing", [this] {
            EXPECT_EQ(Read(0).Actions().Count(), 2);
            EXPECT_FALSE(Read(0).Conditions().Has(EConditionKind::Prone));
        });
        io_.ExpectName(0, "target", "Wolf A");
        io_.ExpectName(0, "source", "Longsword");
        io_.ExpectValue(0, EDamageType::Slashing);
        rng_.ExpectCall(20, 20);
        rng_.ExpectCall(8, 4);
        rng_.ExpectCall(8, 4);
        Action(2, "UseStride", [this] {
            EXPECT_TRUE(Read(1).Dead());
            EXPECT_EQ(Read(1).Hitpoints().Current(), 0);
            EXPECT_EQ(Read(2).Hitpoints().Current(), 24);
            EXPECT_EQ(Read(2).Actions().Count(), 3);
            EXPECT_EQ(Read(0).AttacksMade(), 0);
        });
        io_.ExpectValue(2, EMovementChoice::ContinueMovement);
        io_.ExpectValue(2, TPosition{2, 2});
        io_.ExpectName(0, "reaction", "ReactiveStrike", true);
        io_.ExpectName(0, "source", "Longsword");
        io_.ExpectValue(0, EDamageType::Slashing);
        rng_.ExpectCall(20, 20);
        rng_.ExpectCall(8, 8);
    }

    void WolvesVictorySecondRound()
    {
        Strike(1, "Fighter", "Jaws", 10, 6, 6);
        Knockdown(1, 11);
        Action(1, "End of turn", [this] {
            EXPECT_EQ(Read(0).Hitpoints().Current(), 5);
            EXPECT_TRUE(Read(0).Conditions().Has(EConditionKind::Prone));
            EXPECT_EQ(Read(1).AttacksMade(), 1);
        });
        Strike(2, "Fighter", "Jaws", 10, 6, 3);
    }
    TTransformator* battle_journal_ = nullptr;
    std::optional<TState> checkpoint_;
    std::optional<TAstNode> checkpoint_ast_;
};

TEST_F(TGeneratedEncounterTest, DeclaredFighterDefeatsTwoWolvesWithShieldAndReactiveStrike)
{
    Join(CreateHumanFighter1({.Name = "Fighter"}), 0, {1, 1}, {}, 20);
    Join(CreateWolf({.Name = "Wolf A"}), 1, {2, 1}, {}, 10);
    Join(CreateWolf({.Name = "Wolf B"}), 1, {1, 2}, {}, 5);
    EXPECT_NE(Read(1).Id(), Read(2).Id());
    EXPECT_EQ(Read(0).Actions().Count(), 0);
    EXPECT_EQ(Read(1).Reactions().Count(), 0);
    EXPECT_EQ((*Read(0).Weapons().begin())->Features().Size(), 0U);
    Action(0, "RaiseShield", [this] {
        EXPECT_EQ(Read(0).Actions().Count(), 3);
        EXPECT_EQ(Read(0).Reactions().Count(), 1);
        EXPECT_EQ(Read(0).Flourishes().Count(), 1);
    });
    Strike(0, "Wolf A", "Longsword", 10, 8, 4, true);
    Action(0, "End of turn", [this] {
        EXPECT_EQ(Read(0).Actions().Count(), 1);
        EXPECT_EQ(Read(0).ArmorClass(), 17);
        EXPECT_EQ(Read(1).Hitpoints().Current(), 16);
        EXPECT_EQ(Read(2).Hitpoints().Current(), 24);
    });
    Strike(1, "Fighter", "Jaws", 10, 6, 6);
    io_.ExpectName(0, "reaction", "UseShieldBlock", true, [this](const TAlternatives&) {
        EXPECT_EQ(Read(0).Hitpoints().Current(), 21);
        EXPECT_EQ(Read(0).Reactions().Count(), 1);
    });
    Knockdown(1, 11);
    Action(1, "End of turn", [this] {
        EXPECT_EQ(Read(0).Hitpoints().Current(), 18);
        EXPECT_EQ(Read(0).Shield().Hitpoints(), 17);
        EXPECT_EQ(Read(0).Reactions().Count(), 0);
        EXPECT_TRUE(Read(0).Conditions().Has(EConditionKind::Prone));
        EXPECT_EQ(Read(1).AttacksMade(), 1);
        EXPECT_EQ(Read(1).Actions().Count(), 1);
    });
    Strike(2, "Fighter", "Jaws", 2, 0, 0);
    Action(2, "End of turn", [this] {
        EXPECT_EQ(Read(0).Hitpoints().Current(), 18);
        EXPECT_EQ(Read(1).AttacksMade(), 0);
        EXPECT_EQ(Read(2).AttacksMade(), 1);
        EXPECT_EQ(Read(1).Actions().Count(), 0);
    });
    FighterVictorySecondRound();
    battle_->StartBattle();
    EXPECT_EQ(battle_->Winner(), 0);
    EXPECT_EQ(Read(0).Hitpoints().Current(), 18);
    EXPECT_EQ(Read(0).Shield().Hitpoints(), 17);
    EXPECT_EQ(Read(0).Reactions().Count(), 0);
    EXPECT_EQ(Read(0).Actions().Count(), 0);
    EXPECT_TRUE(Read(1).Dead());
    EXPECT_TRUE(Read(2).Dead());
    EXPECT_EQ(Player(2).GetPosition(), (TPosition{1, 2}));
    EXPECT_EQ(battle_->ElapsedSeconds(), 12U);
    EXPECT_EQ(io_.Suspensions(), 2U);
    ReplayRemainingBattle([this] { FighterVictorySecondRound(); });
}

TEST_F(TGeneratedEncounterTest, ArbitraryMartialWeaponAndGeneralTrainingFightWithoutCopiedActionLists)
{
    THumanFighter1Options options;
    options.Name = "Fighter";
    options.Equipment = std::vector<TWeapon>{TWeapon::Create({
        .Name = "Declared greatblade", .BaseDiceSize = 12,
        .DamageType = EDamageType::Slashing, .Category = EWeaponCategory::Martial,
        .Reach = 1, .HandsRequired = 2,
    })};
    options.ReadiedShield = TShield::Create({});
    options.AncestryFeat = TFeatureSelection("remaster_profiles.GeneralTraining");
    options.ClassFeat = TFeatureSelection("abilities.ViciousSwing");
    options.HeritageFeat = TFeatureSelection("remaster_profiles.Fleet");
    options.TrainingFeat = TFeatureSelection("abilities.Toughness");
    options.AdditionalLanguage = TLanguage::Create({.Name = "Orcish"});
    Join(CreateHumanFighter1(options), 0, {1, 1}, {}, 10);
    Join(CreateWolf({.Name = "Wolf A"}), 1, {2, 1}, {}, 20);
    Join(CreateWolf({.Name = "Wolf B"}), 1, {1, 2}, {}, 10);
    EXPECT_EQ(Read(0).Hitpoints().Current(), 21);
    EXPECT_EQ(Read(0).Movement(), 6);
    EXPECT_EQ(Read(0).Hands().Count(), 0);
    EXPECT_EQ((*Read(0).Weapons().begin())->Features().Size(), 0U);
    EXPECT_NE(Read(1).Id(), Read(2).Id());
    Strike(1, "Fighter", "Jaws", 10, 6, 2);
    Knockdown(1, 11);
    Action(1, "End of turn", [this] {
        EXPECT_EQ(Read(0).Hitpoints().Current(), 17);
        EXPECT_EQ(Read(1).AttacksMade(), 1);
    });
    Strike(2, "Fighter", "Jaws", 10, 6, 2);
    Action(2, "End of turn", [this] {
        EXPECT_EQ(Read(0).Hitpoints().Current(), 13);
        EXPECT_EQ(Read(1).Hitpoints().Current(), 24);
        EXPECT_EQ(Read(2).Hitpoints().Current(), 24);
    });
    Action(0, "UseStand", [this] {
        EXPECT_EQ(Read(0).Actions().Count(), 3);
        EXPECT_EQ(Read(0).Reactions().Count(), 1);
    });
    Strike(0, "Wolf A", "Declared greatblade", 10, 12, 4);
    Action(0, "End of turn", [this] {
        EXPECT_EQ(Read(0).Actions().Count(), 1);
        EXPECT_EQ(Read(1).Hitpoints().Current(), 16);
        EXPECT_EQ(Read(2).Hitpoints().Current(), 24);
    });
    WolvesVictorySecondRound();
    battle_->StartBattle();
    EXPECT_EQ(battle_->Winner(), 1);
    EXPECT_EQ(Read(0).Hitpoints().Current(), 0);
    EXPECT_EQ(Read(0).DyingValue(), 1);
    EXPECT_FALSE(Read(0).Dead());
    EXPECT_FALSE(Read(0).CanAct());
    EXPECT_EQ(Read(1).Hitpoints().Current(), 16);
    EXPECT_EQ(Read(2).Hitpoints().Current(), 24);
    EXPECT_EQ(battle_->ElapsedSeconds(), 12U);
    ReplayRemainingBattle([this] { WolvesVictorySecondRound(); });
}

}
