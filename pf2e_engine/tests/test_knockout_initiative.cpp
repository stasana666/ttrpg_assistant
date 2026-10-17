#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/rules.h>
#include <pf2e_engine/inventory/abilities.h>

#include "test_lib/mock_dice_roller.h"
#include "test_lib/mock_interaction_system.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {

class TKnockoutInitiativeTest : public ::testing::TestWithParam<EZeroHpRule> {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>(*CreateAbilityRules());
        rules->Catalog.Register({"knockout.Checkpoint", EAbilityKind::Action, {}});
        rules->Catalog.Register({"knockout.Damage", EAbilityKind::Action, {}});
        rules->Catalog.Register({"knockout.Training", EAbilityKind::Feature,
                                 {"knockout.Checkpoint", "knockout.Damage"}});
        rules->Executions.Register("knockout.Checkpoint", std::make_shared<TAction>(
            [this](std::shared_ptr<TActionContext> context) { journal_ = context->transformator; },
            TAction::TBoundResources{}, "Checkpoint"));
        rules->Executions.Register("knockout.Damage", std::make_shared<TAction>(
            [this](std::shared_ptr<TActionContext> context) { Damage(std::move(context)); },
            TAction::TBoundResources{{
                .Key = "self.actions", .Count = 1,
                .Read = [](const TPlayer& actor) -> const TResource& {
                    return actor.GetCreature()->Actions();
                },
                .Write = [](TPlayer& actor) { return actor.GetCreature()->Actions(); },
            }}, "Damage"));
        battle_ = std::make_unique<TBattle>(TBattleMap(3, 1), &rng_, io_, std::move(rules));
        for (int id = 0; id < 3; ++id) {
            TIdCollection<TFeatureSelection> features;
            features.Add(TFeatureSelection("knockout.Training"));
            creatures_.push_back(TCreature::Create({
                .PerceptionBaseline = TStatisticBaseline::Create({.IsFixed = true, .Value = 0}),
                .Hitpoints = THitPoints::Create({
                    .Maximum = Statistic::Create({.Base = 20}), .Current = id == 1 ? 5 : 20,
                }),
                .Features = std::move(features),
                .ZeroHpRule = id == 1 ? GetParam() : EZeroHpRule::Dying,
            }));
            rng_.ExpectCall(20, 20 - id * 5);
            battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{id == 0 ? 0 : 1},
                                      TPlayerId{id}, "Participant", ""), {id, 0});
        }
    }

    TPlayer& Player(int id)
    {
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    void Damage(std::shared_ptr<TActionContext> context)
    {
        RunAbilitiesApplyResolvedCreatureDamage(context, BindActionEntity<TCreature>(Player(1)),
            5, TDamageContext::Create({}), [] {});
    }

    TAstNode Snapshot() const
    {
        TAstContext context;
        return battle_->GetAst(context);
    }
    TMockInteractionSystem io_;
    TMockRng rng_;
    std::deque<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
    TTransformator* journal_ = nullptr;
};

TEST_P(TKnockoutInitiativeTest, DeclaredDamageRelocatesRecoveryAndWholeBattleStateReplays)
{
    const auto policy = GetParam();
    std::optional<TState> checkpoint;
    std::optional<TAstNode> before;
    std::optional<TAstNode> after_damage;
    io_.ExpectChoice(0, "next action", "Checkpoint");
    io_.AddCheckCallback([&] {
        ASSERT_NE(journal_, nullptr);
        checkpoint = journal_->CurrentState();
        before = Snapshot();
    });
    io_.ExpectChoice(0, "next action", "Damage");
    io_.AddCheckCallback([&] {
        EXPECT_EQ(battle_->CurrentTurnPlayer(), &Player(0));
        EXPECT_EQ(creatures_[1].Hitpoints()->Current(), 0);
        EXPECT_EQ(creatures_[0].Actions()->Count(), 2);
        EXPECT_TRUE(battle_->AvailableActions(Player(1)).empty());
        after_damage = Snapshot();
    });
    io_.ExpectChoice(0, "next action", "End of turn");
    io_.AddCheckCallback([&] {
        EXPECT_EQ(battle_->CurrentTurnPlayer(), &Player(2));
        EXPECT_EQ(creatures_[1].DyingValue(), policy == EZeroHpRule::Dying ? 1 : 0);
        EXPECT_EQ(creatures_[1].Reactions()->Count(), policy == EZeroHpRule::Defeat ? 1 : 0);
    });
    io_.ExpectChoice(2, "next action", "End of turn");
    if (policy == EZeroHpRule::Dying) {
        rng_.ExpectCall(20, 10);
    }
    io_.AddCheckCallback([&] {
        EXPECT_EQ(battle_->CurrentTurnPlayer(), &Player(0));
        EXPECT_EQ(creatures_[1].DyingValue(), policy == EZeroHpRule::Dying ? 2 : 0);
        EXPECT_EQ(creatures_[1].Reactions()->Count(), policy == EZeroHpRule::Death ? 0 : 1);
    });
    EXPECT_THROW(battle_->StartBattle(), TTooManyCallsError);
    ASSERT_TRUE(checkpoint);
    ASSERT_TRUE(before);
    ASSERT_TRUE(after_damage);
    journal_->Undo(*checkpoint);
    EXPECT_EQ(Snapshot(), *before);
    const auto options = battle_->AvailableActions(Player(0));
    const auto damage = std::ranges::find_if(options, [](const TAbilityOption& option) {
        return option.DefinitionId == "knockout.Damage";
    });
    ASSERT_NE(damage, options.end());
    EXPECT_EQ(battle_->Execute(*damage), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Snapshot(), *after_damage);
    journal_->Undo(*checkpoint);
    EXPECT_EQ(Snapshot(), *before);
    rng_.Verify();
    io_.Verify();
}

INSTANTIATE_TEST_SUITE_P(ZeroHpRules, TKnockoutInitiativeTest,
    ::testing::Values(EZeroHpRule::Defeat, EZeroHpRule::Dying, EZeroHpRule::Death));

}
