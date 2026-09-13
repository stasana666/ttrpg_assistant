#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/player.h>
#include <cpp_config.h>

#include "../test_lib/mock_dice_roller.h"
#include "../test_lib/mock_interaction_system.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

const std::filesystem::path kGrantDataPath{kRootDirPath + "/pf2e_engine/data"};

std::vector<TAbilityOption> Matching(const std::vector<TAbilityOption>& options,
                                    std::string_view definition)
{
    std::vector<TAbilityOption> result;
    for (const auto& option : options) {
        if (option.DefinitionId == definition) {
            result.push_back(option);
        }
    }
    return result;
}

class TGrantedCombatTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        directory_ = std::filesystem::temp_directory_path() /
            ("ttrpg-granted-combat-" + std::to_string(stamp));
        ASSERT_TRUE(std::filesystem::create_directory(directory_));
        for (const auto& entry : std::filesystem::recursive_directory_iterator(kGrantDataPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                factory_.AddSource(entry.path());
            }
        }
        battle_ = std::make_unique<TBattle>(factory_.Create<TBattleMap>(
            TGameObjectIdManager::Instance().Register("simple_battle_map")), &rng_, io_);
    }

    void TearDown() override
    {
        rng_.Verify();
        io_.Verify();
        std::filesystem::remove_all(directory_);
    }

    TCreature Create(std::string name)
    {
        return factory_.Create<TCreature>(TGameObjectIdManager::Instance().Register(name));
    }

    nlohmann::json WarriorDefinition() const
    {
        std::ifstream input(kGrantDataPath / "creatures" / "warrior.json");
        return nlohmann::json::parse(input);
    }

    void RegisterObject(nlohmann::json definition, const std::string& name)
    {
        definition["name"] = name;
        const auto path = directory_ / (name + ".json");
        std::ofstream output(path);
        output.exceptions(std::ios::failbit | std::ios::badbit);
        output << definition;
        output.close();
        factory_.AddSource(path);
    }

    TPlayer& Add(TCreature creature, int id, int team, std::string name, TPosition position)
    {
        creatures_.push_back(std::move(creature));
        rng_.ExpectCall(20, 10);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{team}, TPlayerId{id},
                                   std::move(name), ""), position);
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    void GiveActions(TPlayer& player, int count = 3)
    {
        TTransformator transformator(io_);
        transformator.AddResource(player.GetCreature()->ResourceFor(EResourceKind::Action), count);
    }

    TGameObjectFactory factory_;
    TMockRng rng_;
    TMockInteractionSystem io_;
    std::deque<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
    std::filesystem::path directory_;
};

TEST_F(TGrantedCombatTest, NamedWarriorAndTwoWolvesUseGeneratedGrantsInBattle)
{
    TPlayer& fighter = Add(Create("warrior"), 0, 0, "Fighter", {0, 0});
    TPlayer& first_wolf = Add(Create("wolf"), 1, 1, "Wolf A", {1, 0});
    TPlayer& second_wolf = Add(Create("wolf"), 2, 1, "Wolf B", {0, 1});
    EXPECT_TRUE(battle_->AvailableActions(fighter).empty());
    GiveActions(fighter);
    GiveActions(first_wolf);
    GiveActions(second_wolf);

    const auto fighter_options = battle_->AvailableActions(fighter);
    const auto first_options = battle_->AvailableActions(first_wolf);
    const auto second_options = battle_->AvailableActions(second_wolf);
    EXPECT_EQ(fighter_options.size(), 5u);
    EXPECT_EQ(first_options.size(), 2u);
    EXPECT_EQ(second_options.size(), 2u);
    EXPECT_EQ(Matching(fighter_options, "abilities.Trip").size(), 1u);
    EXPECT_TRUE(Matching(first_options, "abilities.Trip").empty());
    const auto first_attacks = Matching(first_options, "abilities.AttackWithWeapon");
    const auto second_attacks = Matching(second_options, "abilities.AttackWithWeapon");
    ASSERT_EQ(first_attacks.size(), 1u);
    ASSERT_EQ(second_attacks.size(), 1u);
    EXPECT_EQ(first_attacks.front().ActorId, 1);
    EXPECT_EQ(second_attacks.front().ActorId, 2);
    EXPECT_EQ(first_attacks.front().Source, TAbilitySourcePath{}.Field("features").Entry(0));
    EXPECT_EQ(first_attacks.front().Provenance, second_attacks.front().Provenance);
    EXPECT_FALSE(first_attacks.front().Provenance.empty());
    EXPECT_NE(first_wolf.GetCreature(), second_wolf.GetCreature());
}

TEST_F(TGrantedCombatTest, SourceBoundStrikeUsesChosenWeaponAndOnlyPromptsForTarget)
{
    auto definition = WarriorDefinition();
    auto& data = definition["pf2e_creature"]["creature_data"];
    data["features"] = nlohmann::json::array();
    data["weapons"] = nlohmann::json::array({
        {{"name", "Small dagger"}, {"base_dice_size", 4}, {"damage_type", "Piercing"},
         {"category", "Simple"}, {"reach", 1},
         {"features", nlohmann::json::array({"abilities.WeaponStrike"})}},
        {{"name", "Heavy blade"}, {"base_dice_size", 12}, {"damage_type", "Slashing"},
         {"category", "Martial"}, {"reach", 1},
         {"features", nlohmann::json::array({"abilities.WeaponStrike"})}}
    });
    RegisterObject(std::move(definition), "granted_blades_fighter");
    TPlayer& fighter = Add(Create("granted_blades_fighter"), 0, 0, "Fighter", {0, 0});
    TPlayer& first_wolf = Add(Create("wolf"), 1, 1, "Wolf A", {1, 0});
    TPlayer& second_wolf = Add(Create("wolf"), 2, 1, "Wolf B", {0, 1});
    GiveActions(fighter);
    const auto options = Matching(battle_->AvailableActions(fighter), "abilities.Strike");
    ASSERT_EQ(options.size(), 2u);
    const auto heavy_source = TAbilitySourcePath{}.Field("weapons").Entry(1)
        .Field("features").Entry(0);
    const auto heavy = std::find_if(options.begin(), options.end(), [&](const TAbilityOption& option) {
        return option.Source == heavy_source;
    });
    ASSERT_NE(heavy, options.end());
    const int first_hp = first_wolf.GetCreature()->Hitpoints()->Current();
    const int second_hp = second_wolf.GetCreature()->Hitpoints()->Current();

    io_.ExpectChoice(0, "target", "Wolf A");
    rng_.ExpectCall(20, 10);
    rng_.ExpectCall(12, 7);
    EXPECT_EQ(battle_->Execute(*heavy), EAbilityExecutionResult::Applied);

    EXPECT_EQ(first_wolf.GetCreature()->Hitpoints()->Current(), first_hp - 11);
    EXPECT_EQ(second_wolf.GetCreature()->Hitpoints()->Current(), second_hp);
    EXPECT_EQ(fighter.GetCreature()->ResourceFor(EResourceKind::Action)->Count(), 2);
    EXPECT_EQ(fighter.GetCreature()->Get(EConditionKind::MultipleAttackPenalty), 5);
    EXPECT_EQ(first_wolf.GetCreature()->ResourceFor(EResourceKind::Action)->Count(), 0);
    EXPECT_EQ(second_wolf.GetCreature()->ResourceFor(EResourceKind::Action)->Count(), 0);
}

TEST_F(TGrantedCombatTest, ChangingClassSelectionRejectsStaleOptionBeforeCostAndUndoRestoresIt)
{
    auto definition = WarriorDefinition();
    auto& data = definition["pf2e_creature"]["creature_data"];
    data["features"] = nlohmann::json::array();
    RegisterObject({{"type", "pf2e_class"}, {"pf2e_class", {
        {"hitpoints", 10},
        {"features", nlohmann::json::array({"abilities.HumanWarriorActions"})}
    }}}, "granted_fighter_class");
    data["class"] = "granted_fighter_class";
    RegisterObject(std::move(definition), "granted_class_fighter");
    TPlayer& first = Add(Create("granted_class_fighter"), 0, 0, "First fighter", {0, 0});
    TPlayer& second = Add(Create("granted_class_fighter"), 1, 0, "Second fighter", {0, 1});
    Add(Create("wolf"), 2, 1, "Wolf", {1, 0});
    GiveActions(first);
    GiveActions(second);
    const auto choices = Matching(battle_->AvailableActions(first), "abilities.Stride");
    ASSERT_EQ(choices.size(), 1u);
    const TAbilityOption saved = choices.front();
    EXPECT_EQ(saved.Source, TAbilitySourcePath{}.Field("class").Field("features").Entry(0));
    TTransformator transformator(io_);
    auto ctx = transformator.ExecutionContext();
    const auto initial = ctx.GetState();
    auto class_guard = first.GetCreature()->Class();
    auto features = ctx.Project(class_guard, [](TClass& definition) { return definition.Features(); });
    auto selected = ctx.Project(features, [](TIdCollection<TFeatureSelection>& values) {
        return TGuarded<TFeatureSelection>(*values.Find(TId<TFeatureSelection>{0}));
    });
    ctx.AssignGuarded(selected, TFeatureSelection{}, class_guard, "features.0");

    EXPECT_TRUE(battle_->AvailableActions(first).empty());
    EXPECT_EQ(battle_->AvailableActions(second).size(), 5u);
    EXPECT_EQ(battle_->Execute(saved), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(first.GetCreature()->ResourceFor(EResourceKind::Action)->Count(), 3);
    EXPECT_EQ(second.GetCreature()->ResourceFor(EResourceKind::Action)->Count(), 3);

    ctx.Undo(initial);
    const auto restored = Matching(battle_->AvailableActions(first), "abilities.Stride");
    ASSERT_EQ(restored.size(), 1u);
    EXPECT_EQ(restored.front().Source, saved.Source);
    EXPECT_EQ(restored.front().Provenance, saved.Provenance);
    EXPECT_EQ(battle_->AvailableActions(second).size(), 5u);
}

TEST_F(TGrantedCombatTest, WeaponRequiredAbilityIsUnavailableFromCreatureFeature)
{
    auto definition = WarriorDefinition();
    auto& data = definition["pf2e_creature"]["creature_data"];
    data["features"] = nlohmann::json::array({"abilities.WeaponStrike"});
    RegisterObject(std::move(definition), "granted_unbound_fighter");
    TPlayer& fighter = Add(Create("granted_unbound_fighter"), 0, 0, "Fighter", {0, 0});
    Add(Create("wolf"), 1, 1, "Wolf", {1, 0});
    GiveActions(fighter);
    const auto generated = fighter.GetCreature()->CollectAbilities();
    ASSERT_EQ(generated.Grants().size(), 1u);
    EXPECT_EQ(generated.Grants().front().DefinitionId, "abilities.Strike");
    EXPECT_TRUE(battle_->AvailableActions(fighter).empty());
    const auto& grant = generated.Grants().front();
    const TAbilityOption forged{
        .ActorId = fighter.GetId(),
        .ActorIdentity = &fighter,
        .DefinitionId = grant.DefinitionId,
        .Source = grant.Source,
        .Provenance = grant.Provenance,
        .Action = fighter.GetCreature()->AbilityRules().Executions.Require(grant.DefinitionId).Action,
        .Trigger = {},
    };
    EXPECT_EQ(battle_->Execute(forged), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(fighter.GetCreature()->ResourceFor(EResourceKind::Action)->Count(), 3);
}

}
