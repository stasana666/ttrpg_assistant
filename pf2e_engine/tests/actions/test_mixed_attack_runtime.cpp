#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/mechanics/creature.h>
#include <cpp_config.h>

#include "../test_lib/mock_dice_roller.h"
#include "../test_lib/mock_interaction_system.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

TAbilityScores NeutralScores()
{
    const auto score = TAbilityScore::Create({.Value = 10});
    return TAbilityScores::Create({
        .Strength = score, .Dexterity = score, .Constitution = score,
        .Intelligence = score, .Wisdom = score, .Charisma = score,
    });
}

TWeapon MixedWeapon(std::string name, bool agile)
{
    TVariantMap<EWeaponTraitKind, TWeaponTrait> traits;
    if (agile) {
        traits.Set(TWeaponTraitAgile{});
    }
    TIdCollection<TFeatureSelection> features;
    features.Add(TFeatureSelection("abilities.WeaponStrike"));
    return TWeapon::Create({
        .Name = std::move(name), .BaseDiceSize = 4, .DamageDiceCount = agile ? 1 : 2,
        .AttackBonusBaseline = TStatisticBaseline::Create({.IsFixed = true, .Value = 10}),
        .DamageModifierBaseline = TStatisticBaseline::Create({.IsFixed = true, .Value = agile ? 0 : 3}),
        .DamageType = EDamageType::Slashing, .Category = EWeaponCategory::Simple,
        .Traits = std::move(traits), .Features = std::move(features),
    });
}

class TMixedAttackRuntimeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 std::filesystem::path(kRootDirPath) / "pf2e_engine/data")) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                factory_.AddSource(entry.path());
            }
        }
        rules_ = factory_.AbilityRules();
        nlohmann::json map = {{"x_size", 8}, {"y_size", 8}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, rules_);
        journal_ = std::make_unique<TTransformator>(io_);
        actor_ = &Add(true);
        target_ = &Add(false);
    }

    TPlayer& Add(bool actor)
    {
        TIdCollection<TWeapon> weapons;
        TIdCollection<TFeatureSelection> features;
        if (actor) {
            weapons.Add(MixedWeapon("Ordinary", false));
            weapons.Add(MixedWeapon("Agile", true));
            features.Add(TFeatureSelection("abilities.HumanWarriorActions"));
            features.Add(TFeatureSelection("abilities.FighterReaction"));
        } else {
            features.Add(TFeatureSelection("abilities.WolfActions"));
        }
        const auto fixed = TProficiencyValue::Create({.IsFixed = true, .FixedBonus = 10});
        creatures_.push_back(TCreature::Create({
            .Level = 1, .Characteristic = NeutralScores(),
            .Proficiency = TProficiency::Create({
                .Savethrow = std::map<ESavethrow, TProficiencyValue>{{ESavethrow::Reflex, fixed}},
                .Skill = std::map<ESkill, TProficiencyValue>{{ESkill::Athletics, fixed}},
            }),
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = 100}), .Current = 100,
            }),
            .ArmorClassBaseline = TStatisticBaseline::Create({.IsFixed = true, .Value = 20}),
            .Weapons = std::move(weapons), .Features = std::move(features),
            .Actions = TResource(6), .Reactions = TResource(1),
        }));
        const int id = actor ? 0 : 1;
        rng_.ExpectCall(20, actor ? 1 : 20);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{id}, TPlayerId{id},
                                   actor ? "Actor" : "Target", ""), {id, 0});
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    TAbilityOption Option(std::string_view definition, std::optional<std::size_t> weapon = {}) const
    {
        const auto options = battle_->AvailableActions(*actor_);
        const auto found = std::find_if(options.begin(), options.end(), [&](const auto& option) {
            return option.DefinitionId == definition && (!weapon || option.Source.StartsWith(
                TAbilitySourcePath{}.Field("weapons").Entry(*weapon)));
        });
        if (found == options.end()) {
            throw std::logic_error("expected mixed combat action is unavailable");
        }
        return *found;
    }

    void Execute(const TAbilityOption& option, TAbilityExecutionBindings bindings = {})
    {
        auto context = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(), .dice_roller = &rng_, .transformator = journal_.get(),
            .io_system = &io_, .scheduler = nullptr, .effect_manager = nullptr,
        });
        EXPECT_EQ(ExecuteAbility(option, context, *actor_,
            CollectAbilities(*actor_->GetCreature(), rules_->Catalog), rules_->Executions,
            std::move(bindings)), EAbilityExecutionResult::Applied);
    }

    void NativeAttack(std::string_view weapon, int natural, bool hit)
    {
        io_.ExpectChoice(0, "weapon", std::string(weapon));
        io_.ExpectChoice(0, "target", "Target");
        rng_.ExpectCall(20, natural);
        if (hit) {
            rng_.ExpectCall(4, 2);
            if (weapon == "Ordinary") {
                rng_.ExpectCall(4, 3);
            }
        }
        Execute(Option("abilities.AttackWithWeapon"));
    }

    void CompiledAgile(int natural, bool hit)
    {
        rng_.ExpectCall(20, natural);
        if (hit) {
            rng_.ExpectCall(4, 2);
        }
        Execute(Option("abilities.Strike", 1));
    }

    void Record(TPlayer& player, int count)
    {
        auto execution = journal_->ExecutionContext();
        execution.Invoke(TGuarded<TCreature>(*player.GetCreature()), &TCreature::RecordAttack, count);
    }

    void TearDown() override
    {
        rng_.Verify();
        io_.Verify();
    }

    TGameObjectFactory factory_;
    TMockRng rng_;
    TMockInteractionSystem io_;
    std::deque<TCreature> creatures_;
    std::shared_ptr<const TAbilityRules> rules_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> journal_;
    TPlayer* actor_ = nullptr;
    TPlayer* target_ = nullptr;
};

TEST_F(TMixedAttackRuntimeTest, LegacyAndCompiledAttacksUseCurrentWeaponPenaltyAndUndoTogether)
{
    const auto initial = journal_->CurrentState();
    NativeAttack("Ordinary", 10, true);
    EXPECT_EQ(target_->GetCreature()->Hitpoints()->Current(), 92);
    EXPECT_EQ(actor_->GetCreature()->AttacksMade().Get(), 1);
    CompiledAgile(14, true);
    EXPECT_EQ(target_->GetCreature()->Hitpoints()->Current(), 90);
    EXPECT_EQ(actor_->GetCreature()->AttacksMade().Get(), 2);
    NativeAttack("Ordinary", 19, false);
    EXPECT_EQ(target_->GetCreature()->Hitpoints()->Current(), 90);
    EXPECT_EQ(actor_->GetCreature()->AttacksMade().Get(), 3);
    EXPECT_EQ(actor_->GetCreature()->Actions()->Count(), 3);

    journal_->Undo(initial);
    EXPECT_EQ(target_->GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_EQ(actor_->GetCreature()->AttacksMade().Get(), 0);
    EXPECT_EQ(actor_->GetCreature()->Actions()->Count(), 6);
}

TEST_F(TMixedAttackRuntimeTest, AgileThenOrdinaryUsesFiveAndLaterAgileUsesEight)
{
    CompiledAgile(10, true);
    NativeAttack("Ordinary", 14, false);
    EXPECT_EQ(actor_->GetCreature()->AttacksMade().Get(), 2);
    CompiledAgile(18, true);
    CompiledAgile(18, true);
    EXPECT_EQ(actor_->GetCreature()->AttacksMade().Get(), 4);
    EXPECT_EQ(target_->GetCreature()->Hitpoints()->Current(), 94);
}

TEST_F(TMixedAttackRuntimeTest, TripAfterAgileUsesNonAgilePenaltyAndCountsForNextStrike)
{
    CompiledAgile(10, true);
    io_.ExpectChoice(0, "target", "Target");
    rng_.ExpectCall(20, 14);
    Execute(Option("abilities.Trip"));
    EXPECT_EQ(ConditionValue(*target_->GetCreature(), EConditionKind::Prone), 0);
    EXPECT_EQ(ConditionValue(*actor_->GetCreature(), EConditionKind::Prone), 0);
    EXPECT_EQ(actor_->GetCreature()->AttacksMade().Get(), 2);
    CompiledAgile(18, true);
    EXPECT_EQ(target_->GetCreature()->Hitpoints()->Current(), 96);
    EXPECT_EQ(actor_->GetCreature()->AttacksMade().Get(), 3);
}

TEST_F(TMixedAttackRuntimeTest, MissesInBothImplementationsStillCount)
{
    CompiledAgile(2, false);
    NativeAttack("Agile", 2, false);
    EXPECT_EQ(actor_->GetCreature()->AttacksMade().Get(), 2);
    EXPECT_EQ(target_->GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_EQ(actor_->GetCreature()->Actions()->Count(), 4);
}

TEST_F(TMixedAttackRuntimeTest, OpportunityIgnoresExistingPenaltyAndDoesNotRecordAttack)
{
    Record(*actor_, 2);
    const auto initial = journal_->CurrentState();
    const auto options = battle_->AvailableReactions({ETrigger::OnMove, target_});
    ASSERT_EQ(options.size(), 1);
    io_.ExpectChoice(0, "weapon", "Ordinary");
    rng_.ExpectCall(20, 10);
    rng_.ExpectCall(4, 2);
    rng_.ExpectCall(4, 3);
    Execute(options.front(), {.TriggerActor = target_});
    EXPECT_EQ(target_->GetCreature()->Hitpoints()->Current(), 92);
    EXPECT_EQ(actor_->GetCreature()->AttacksMade().Get(), 2);
    EXPECT_EQ(actor_->GetCreature()->Actions()->Count(), 6);
    EXPECT_EQ(actor_->GetCreature()->Reactions()->Count(), 0);
    journal_->Undo(initial);
    EXPECT_EQ(target_->GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_EQ(actor_->GetCreature()->AttacksMade().Get(), 2);
    EXPECT_EQ(actor_->GetCreature()->Reactions()->Count(), 1);
}

TEST_F(TMixedAttackRuntimeTest, BattleResetsAttacksBeforeStartAndAfterEndOfEachTurn)
{
    journal_->ReduceResource(actor_->GetCreature()->Actions(), 6);
    journal_->ReduceResource(target_->GetCreature()->Actions(), 6);
    Record(*actor_, 2);
    Record(*target_, 2);
    io_.AddCheckCallback([&] {
        EXPECT_EQ(actor_->GetCreature()->AttacksMade().Get(), 0);
        EXPECT_EQ(target_->GetCreature()->AttacksMade().Get(), 2);
    });
    io_.ExpectChoice(0, "next action", "attack_with_weapon");
    io_.ExpectChoice(0, "weapon", "Ordinary");
    io_.ExpectChoice(0, "target", "Target");
    rng_.ExpectCall(20, 10);
    rng_.ExpectCall(4, 2);
    rng_.ExpectCall(4, 3);
    io_.AddCheckCallback([&] {
        EXPECT_EQ(actor_->GetCreature()->AttacksMade().Get(), 1);
    });
    io_.ExpectChoice(0, "next action", "End of turn");
    io_.AddCheckCallback([&] {
        EXPECT_EQ(actor_->GetCreature()->AttacksMade().Get(), 0);
        EXPECT_EQ(target_->GetCreature()->AttacksMade().Get(), 0);
    });
    io_.ExpectChoice(1, "next action", "End of turn");
    EXPECT_THROW(battle_->StartBattle(), TTooManyCallsError);
    EXPECT_EQ(target_->GetCreature()->Hitpoints()->Current(), 92);
}

}
