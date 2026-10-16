#include <pf2e_engine/action_blocks/add_condition.h>
#include <pf2e_engine/action_blocks/deal_damage.h>
#include <pf2e_engine/action_blocks/flat_damage_roll.h>
#include <pf2e_engine/action_blocks/get_targets_in_area.h>
#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/mechanics/creature.h>
#include <pf2e_engine/mechanics/damage.h>

#include "../test_lib/mock_dice_roller.h"
#include "../test_lib/mock_interaction_system.h"

#include <cpp_config.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
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

TGameObjectId Id(std::string_view name)
{
    return TGameObjectIdManager::Instance().Register(std::string(name));
}

class TLegacyDamageContextTest : public ::testing::Test {
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
    }

    void Start(bool nonlethal = false, EZeroHpRule policy = EZeroHpRule::Dying)
    {
        journal_.reset();
        battle_.reset();
        creatures_.clear();
        nlohmann::json map = {{"x_size", 8}, {"y_size", 8}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, rules_);
        journal_ = std::make_unique<TTransformator>(io_);
        actor_ = &Add(0, "Actor", 40, EZeroHpRule::Dying, nonlethal);
        target_ = &Add(1, "Target", 4, policy);
    }

    TPlayer& Add(int id, std::string name, int hitpoints, EZeroHpRule policy,
                 bool nonlethal = false)
    {
        TIdCollection<TWeapon> weapons;
        TIdCollection<TFeatureSelection> features;
        if (id == 0) {
            TVariantMap<EWeaponTraitKind, TWeaponTrait> traits;
            if (nonlethal) {
                traits.Set(TWeaponTraitNonlethal{});
            }
            TIdCollection<TFeatureSelection> weapon_features;
            weapon_features.Add(TFeatureSelection("abilities.WeaponStrike"));
            weapons.Add(TWeapon::Create({
                .Name = "Weapon", .BaseDiceSize = 4,
                .AttackBonusBaseline = TStatisticBaseline::Create({.IsFixed = true, .Value = 10}),
                .DamageModifierBaseline = TStatisticBaseline::Create({.IsFixed = true, .Value = 0}),
                .DamageType = EDamageType::Slashing, .Category = EWeaponCategory::Simple,
                .Traits = std::move(traits), .Features = std::move(weapon_features),
            }));
            features.Add(TFeatureSelection("abilities.HumanWarriorActions"));
        }
        const auto score = TAbilityScore::Create({.Value = 10});
        const auto fixed = TProficiencyValue::Create({.IsFixed = true, .FixedBonus = 10});
        creatures_.push_back(TCreature::Create({
            .Level = 1,
            .Characteristic = TAbilityScores::Create({
                .Strength = score, .Dexterity = score, .Constitution = score,
                .Intelligence = score, .Wisdom = score, .Charisma = score,
            }),
            .Proficiency = TProficiency::Create({
                .Savethrow = std::map<ESavethrow, TProficiencyValue>{{ESavethrow::Reflex, fixed}},
                .Skill = std::map<ESkill, TProficiencyValue>{{ESkill::Athletics, fixed}},
            }),
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = id == 0 ? 40 : 20}), .Current = hitpoints,
            }),
            .ArmorClassBaseline = TStatisticBaseline::Create({.IsFixed = true, .Value = 20}),
            .Weapons = std::move(weapons), .Features = std::move(features), .ZeroHpRule = policy,
            .Actions = TResource(6), .Reactions = TResource(1),
        }));
        rng_.ExpectCall(20, id == 0 ? 20 : 1);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{id}, TPlayerId{id},
                                   std::move(name), ""), {id, 0});
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    std::shared_ptr<TActionContext> Context()
    {
        auto context = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(), .dice_roller = &rng_, .transformator = journal_.get(),
            .io_system = &io_, .scheduler = nullptr, .effect_manager = nullptr,
            .game_object_registry = std::make_shared<TGameObjectRegistry>(),
        });
        context->game_object_registry->Add(Id("self"), actor_);
        context->game_object_registry->Add(Id("target"), target_);
        return context;
    }

    void Execute(std::string_view definition)
    {
        const auto options = battle_->AvailableActions(*actor_);
        const auto found = std::find_if(options.begin(), options.end(), [&](const auto& option) {
            return option.DefinitionId == definition;
        });
        ASSERT_NE(found, options.end());
        EXPECT_EQ(ExecuteAbility(*found, Context(), *actor_,
            CollectAbilities(*actor_->GetCreature(), rules_->Catalog), rules_->Executions),
            EAbilityExecutionResult::Applied);
    }

    void Strike(bool compiled, int natural)
    {
        if (!compiled) {
            io_.ExpectChoice(0, "target", "Target");
        }
        rng_.ExpectCall(20, natural);
        rng_.ExpectCall(4, 4);
        Execute(compiled ? "abilities.Strike" : "abilities.AttackWithWeapon");
    }

    void Flat(const std::shared_ptr<TActionContext>& context,
              std::optional<int> critical = {}, std::optional<int> nonlethal = {})
    {
        TBlockInput input;
        input.Add(Id("dice"), std::string("1d6"));
        input.Add(Id("damage_type"), std::string("Bludgeoning"));
        if (critical) {
            input.Add(Id("critical"), *critical);
        }
        if (nonlethal) {
            input.Add(Id("nonlethal"), *nonlethal);
        }
        FFlatDamageRoll(std::move(input), Id("damage"))(context);
    }

    void Deliver(const std::shared_ptr<TActionContext>& context)
    {
        TBlockInput input;
        input.Add(Id("damage"), Id("damage"));
        input.Add(Id("target"), Id("target"));
        FDealDamage(std::move(input), Id("void"))(context);
    }

    void Condition(std::string name, int value)
    {
        TBlockInput input;
        input.Add(Id("condition"), std::move(name));
        input.Add(Id("value"), value);
        input.Add(Id("target"), Id("target"));
        FAddCondition(std::move(input), Id("void"))(Context());
    }

    TAstNode Snapshot() const
    {
        TAstContext context;
        auto node = TAstNode::MakeObject("legacy_damage_context");
        AddOwnedObject(node, "battle", *battle_, context);
        AddOwnedObject(node, "journal", *journal_, context);
        return node;
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

TEST_F(TLegacyDamageContextTest, LegacyAndCompiledStrikeProduceIdenticalKnockoutContext)
{
    for (const auto policy : {EZeroHpRule::Dying, EZeroHpRule::Death}) {
        for (const bool nonlethal : {false, true}) {
            for (const int natural : {10, 20}) {
                SCOPED_TRACE(::testing::Message() << static_cast<int>(policy)
                             << "/" << nonlethal << "/" << natural);
                Start(nonlethal, policy);
                const auto checkpoint = journal_->CurrentState();
                const auto before = Snapshot();
                Strike(false, natural);
                const auto expected = target_->GetCreature()->ToJson();
                EXPECT_EQ(target_->GetCreature()->Hitpoints()->Current(), 0);
                EXPECT_EQ(target_->GetCreature()->IsDead(), !nonlethal && policy == EZeroHpRule::Death);
                EXPECT_EQ(target_->GetCreature()->DyingValue(),
                          !nonlethal && policy == EZeroHpRule::Dying ? (natural == 20 ? 2 : 1) : 0);
                EXPECT_FALSE(target_->GetCreature()->CanAct());
                journal_->Undo(checkpoint);
                EXPECT_EQ(Snapshot(), before);
                Strike(true, natural);
                EXPECT_EQ(target_->GetCreature()->ToJson(), expected);
                journal_->Undo(checkpoint);
                EXPECT_EQ(Snapshot(), before);
            }
        }
    }
}

TEST_F(TLegacyDamageContextTest, LaterOrdinaryWeaponHitDoesNotReuseCriticalMetadata)
{
    Start();
    const auto checkpoint = journal_->CurrentState();
    const auto before = Snapshot();
    Strike(false, 20);
    ASSERT_EQ(target_->GetCreature()->DyingValue(), 2);
    journal_->Heal(target_, 4);
    ASSERT_EQ(target_->GetCreature()->WoundedValue(), 1);
    Strike(false, 13);
    EXPECT_EQ(target_->GetCreature()->DyingValue(), 2);
    EXPECT_EQ(actor_->GetCreature()->AttacksMade().Get(), 2);
    journal_->Undo(checkpoint);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TLegacyDamageContextTest, TripCriticalDamageCarriesCriticalSuccessToHealthRules)
{
    Start();
    const auto checkpoint = journal_->CurrentState();
    const auto before = Snapshot();
    io_.ExpectChoice(0, "target", "Target");
    rng_.ExpectCall(20, 20);
    rng_.ExpectCall(6, 4);
    Execute("abilities.Trip");
    EXPECT_EQ(target_->GetCreature()->DyingValue(), 2);
    EXPECT_EQ(ConditionValue(*target_->GetCreature(), EConditionKind::Prone), 1);
    journal_->Undo(checkpoint);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TLegacyDamageContextTest, FlatDamageFlagsAreOptionalValidatedAndDeliveredTogether)
{
    Start();
    const auto checkpoint = journal_->CurrentState();
    auto context = Context();
    Flat(context);
    const auto ordinary = std::get<std::shared_ptr<TDamage>>(
        context->game_object_registry->GetGameObjectPtr(Id("damage")));
    EXPECT_FALSE(ordinary->Context().Critical());
    EXPECT_FALSE(ordinary->Context().Nonlethal());
    EXPECT_THROW(Flat(context, 2), std::invalid_argument);
    EXPECT_THROW(Flat(context, 0, -1), std::invalid_argument);
    EXPECT_EQ(std::get<std::shared_ptr<TDamage>>(
        context->game_object_registry->GetGameObjectPtr(Id("damage"))), ordinary);
    Flat(context, 1, 1);
    rng_.ExpectCall(6, 4);
    Deliver(context);
    EXPECT_EQ(target_->GetCreature()->Hitpoints()->Current(), 0);
    EXPECT_EQ(target_->GetCreature()->DyingValue(), 0);
    EXPECT_EQ(ConditionValue(*target_->GetCreature(), EConditionKind::Unconscious), 1);
    journal_->Undo(checkpoint);
}

TEST_F(TLegacyDamageContextTest, DeadTargetIsRejectedBeforeDamageDiceOrJournalWrites)
{
    Start(false, EZeroHpRule::Death);
    Strike(false, 10);
    ASSERT_TRUE(target_->GetCreature()->IsDead());
    const auto before = Snapshot();
    auto context = Context();
    Flat(context);
    EXPECT_THROW(Deliver(context), std::invalid_argument);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TLegacyDamageContextTest, EveryLegacyAreaExcludesDeadButIncludesUnconsciousParticipants)
{
    Start();
    auto& corpse = Add(2, "Corpse", 0, EZeroHpRule::Death);
    journal_->DealDamage(target_, 4);
    ASSERT_TRUE(target_->GetCreature()->NeedsRecoveryCheck());
    ASSERT_TRUE(corpse.GetCreature()->IsDead());
    auto context = Context();
    const auto before = Snapshot();
    for (const std::string type : {"emanation", "burst", "cone", "line"}) {
        SCOPED_TRACE(type);
        TBlockInput input;
        input.Add(Id("type"), type);
        input.Add(Id("center"), Id("self"));
        input.Add(Id("radius"), 8);
        input.Add(Id("range"), 8);
        input.Add(Id("length"), 8);
        input.Add(Id("width"), 1);
        if (type == "burst") {
            io_.ExpectChoice(0, "burst center", "0 0");
        } else if (type != "emanation") {
            io_.ExpectChoice(0, type + " direction", "7 0");
        }
        FGetTargetsInArea(std::move(input), Id("targets"))(context);
        const auto targets = std::get<TPlayerList>(
            context->game_object_registry->GetGameObjectPtr(Id("targets")));
        EXPECT_NE(std::find(targets.begin(), targets.end(), target_), targets.end());
        EXPECT_EQ(std::find(targets.begin(), targets.end(), &corpse), targets.end());
    }
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TLegacyDamageContextTest, NewConditionAdaptersUseGeneratedHealthTransitionsAndUndo)
{
    Start();
    const auto checkpoint = journal_->CurrentState();
    const auto before = Snapshot();
    journal_->DealDamage(target_, 4);
    Condition("Wounded", 1);
    Condition("Dying", 2);
    Condition("Unconscious", 1);
    EXPECT_EQ(target_->GetCreature()->WoundedValue(), 1);
    EXPECT_EQ(target_->GetCreature()->DyingValue(), 2);
    EXPECT_EQ(ConditionValue(*target_->GetCreature(), EConditionKind::Unconscious), 1);
    EXPECT_EQ(ConditionValue(*target_->GetCreature(), EConditionKind::Prone), 1);
    EXPECT_TRUE(target_->GetCreature()->NeedsRecoveryCheck());
    journal_->Undo(checkpoint);
    EXPECT_EQ(Snapshot(), before);
}

}
