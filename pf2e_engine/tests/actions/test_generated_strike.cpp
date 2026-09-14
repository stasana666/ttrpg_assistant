#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <cpp_config.h>

#include "../test_lib/mock_dice_roller.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

const std::filesystem::path kStrikeData{kRootDirPath + "/pf2e_engine/data"};

class TStrikeChoiceIo final : public IInteractionSystem {
public:
    void ExpectTarget(std::string name, bool suspend = false)
    {
        expected_.push_back({std::move(name), suspend});
    }

    void RequestChoice(int player_id, TAlternatives alternatives, TState state,
                       std::function<void(size_t)> on_choice) override
    {
        if (expected_.empty()) {
            throw std::logic_error("unexpected compiled strike choice");
        }
        const bool suspend = expected_.front().Suspend;
        const auto index = ChooseAlternativeIndex(player_id, alternatives);
        if (suspend) {
            throw TSavepointStackUnwind(state,
                [on_choice = std::move(on_choice), index]() { on_choice(index); });
        }
        on_choice(index);
    }

    void HandleReactionTrigger(const TTriggerContext&, const TState&) override {}
    std::ostream& GameLog() override { return log_; }
    std::ostream& DevLog() override { return log_; }
    void Verify() const { EXPECT_TRUE(expected_.empty()); }
    const std::vector<std::string>& Candidates() const { return candidates_; }

protected:
    size_t ChooseAlternativeIndex(int player_id, const TAlternatives& alternatives) override
    {
        EXPECT_EQ(player_id, 0);
        EXPECT_EQ(alternatives.Kind(), "target");
        if (expected_.empty()) {
            throw std::logic_error("unexpected compiled strike alternative");
        }
        const auto expected = expected_.front();
        expected_.pop_front();
        candidates_.clear();
        std::optional<size_t> result;
        for (size_t index = 0; index < alternatives.Size(); ++index) {
            candidates_.push_back(alternatives[index].name);
            if (alternatives[index].name == expected.Name) {
                result = index;
            }
        }
        if (!result) {
            throw std::logic_error("expected target is absent: " + expected.Name);
        }
        return *result;
    }

private:
    struct TExpected {
        std::string Name;
        bool Suspend;
    };

    std::deque<TExpected> expected_;
    std::vector<std::string> candidates_;
    std::ostringstream log_;
};

nlohmann::json StrikeWeapon(int die = 6, int reach = 1)
{
    return {
        {"name", "Blade"}, {"base_dice_size", die},
        {"damage_type", "Piercing"}, {"category", "Simple"}, {"reach", reach},
        {"features", nlohmann::json::array({"abilities.WeaponStrike"})}
    };
}

class TGeneratedStrikeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        directory_ = std::filesystem::temp_directory_path() /
            ("ttrpg-generated-strike-" + std::to_string(stamp));
        ASSERT_TRUE(std::filesystem::create_directory(directory_));
        for (const auto& entry : std::filesystem::recursive_directory_iterator(kStrikeData)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                factory_.AddSource(entry.path());
            }
        }
        nlohmann::json map = {{"x_size", 8}, {"y_size", 8}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, factory_.AbilityRules());
        journal_ = std::make_unique<TTransformator>(io_);
    }

    void TearDown() override
    {
        rng_.Verify();
        io_.Verify();
        std::filesystem::remove_all(directory_);
    }

    static nlohmann::json Definition(bool wolf = false)
    {
        std::ifstream input(kStrikeData / "creatures" / (wolf ? "wolf.json" : "warrior.json"));
        auto result = nlohmann::json::parse(input);
        auto& envelope = result["pf2e_creature"];
        envelope.erase("feats");
        auto& data = envelope["creature_data"];
        data["features"] = nlohmann::json::array();
        data["weapons"] = nlohmann::json::array();
        data["natural_weapons"] = nlohmann::json::array();
        data["actions"] = 3;
        data["hitpoints"] = {
            {"maximum", {{"base", 100}}}, {"current", 100}, {"temporary", 0}
        };
        return result;
    }

    static nlohmann::json Armed(nlohmann::json weapons = nlohmann::json::array({StrikeWeapon()}))
    {
        auto result = Definition();
        result["pf2e_creature"]["creature_data"]["weapons"] = std::move(weapons);
        return result;
    }

    static nlohmann::json Natural(bool pack = false, int reach = 1)
    {
        auto result = Definition(true);
        auto weapon = StrikeWeapon(6, reach);
        weapon["name"] = "Jaws";
        weapon["category"] = "Unarmed";
        weapon["attack_bonus_baseline"] = {{"is_fixed", true}, {"value", 9}};
        weapon["damage_modifier_baseline"] = {{"is_fixed", true}, {"value", 2}};
        auto& data = result["pf2e_creature"]["creature_data"];
        data["natural_weapons"] = nlohmann::json::array({weapon});
        if (pack) {
            data["features"] = nlohmann::json::array({"abilities.PackAttack"});
        }
        return result;
    }

    static nlohmann::json Target(int ac = 15, int temporary = 0)
    {
        auto result = Definition();
        auto& data = result["pf2e_creature"]["creature_data"];
        data["armor_class_baseline"] = {{"is_fixed", true}, {"value", ac}};
        data["hitpoints"]["temporary"] = temporary;
        data["actions"] = 0;
        return result;
    }

    TPlayer& Add(nlohmann::json definition, std::string name, TPosition position, int team = 1)
    {
        const int id = static_cast<int>(creatures_.size());
        const auto object_name = "compiled_strike_" + std::to_string(id);
        definition["name"] = object_name;
        const auto path = directory_ / (object_name + ".json");
        std::ofstream output(path);
        output.exceptions(std::ios::failbit | std::ios::badbit);
        output << definition;
        output.close();
        factory_.AddSource(path);
        creatures_.push_back(factory_.Create<TCreature>(
            TGameObjectIdManager::Instance().Register(object_name)));
        rng_.ExpectCall(20, 10);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{team}, TPlayerId{id},
                                   std::move(name), ""), position);
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    TAbilityOption Option(TPlayer& actor, std::size_t entry = 0, bool natural = false) const
    {
        const auto source = TAbilitySourcePath{}.Field(natural ? "natural_weapons" : "weapons")
            .Entry(entry).Field("features").Entry(0);
        const auto options = battle_->AvailableActions(actor);
        const auto found = std::ranges::find_if(options, [&](const TAbilityOption& option) {
            return option.DefinitionId == "abilities.Strike" && option.Source == source;
        });
        if (found == options.end()) {
            throw std::logic_error("missing compiled strike option");
        }
        return *found;
    }

    EAbilityExecutionResult ExecuteJournal(TPlayer& actor, const TAbilityOption& option)
    {
        const auto& rules = battle_->AbilityRules();
        auto context = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(), .dice_roller = &rng_, .transformator = journal_.get(),
            .io_system = &io_, .scheduler = nullptr, .effect_manager = nullptr,
        });
        return ExecuteAbility(option, std::move(context), actor,
            CollectAbilities(*actor.GetCreature(), rules.Catalog), rules.Executions,
            {.ValidateSource = [this, &actor, source = option.Source]() {
                const auto fresh = CollectAbilities(*actor.GetCreature(), battle_->AbilityRules().Catalog);
                return std::ranges::any_of(fresh.Grants(), [&](const TGrantedAbility& grant) {
                    return grant.DefinitionId == "abilities.Strike" && grant.Source == source;
                });
            }});
    }

    static TSavepointStackUnwind Suspend(const std::function<void()>& work)
    {
        try {
            work();
        } catch (const TSavepointStackUnwind& point) {
            return point;
        }
        throw std::logic_error("compiled strike did not suspend");
    }

    TAstNode Snapshot() const
    {
        TAstContext context;
        auto node = TAstNode::MakeObject("compiled_strike_execution");
        AddOwnedObject(node, "journal", *journal_, context);
        AddOwnedObject(node, "battle", *battle_, context);
        return node;
    }

    TGameObjectFactory factory_;
    TMockRng rng_;
    TStrikeChoiceIo io_;
    std::deque<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> journal_;
    std::filesystem::path directory_;
};

TEST_F(TGeneratedStrikeTest, FactoryPcUsesTheSelectedSameNameWeaponAndOnlyPromptsForTarget)
{
    auto& actor = Add(Armed(nlohmann::json::array({StrikeWeapon(4), StrikeWeapon(12)})),
                      "Fighter", {0, 0}, 0);
    auto& first = Add(Target(), "First", {1, 0});
    auto& second = Add(Target(), "Second", {0, 1});
    const auto before = Snapshot();
    ASSERT_EQ(battle_->AvailableActions(actor).size(), 2u);
    const auto light = Option(actor);
    const auto heavy = Option(actor, 1);
    const auto& heavy_weapon = *actor.GetCreature()->Weapons()->Find(TId<TWeapon>{1});
    EXPECT_EQ(actor.GetCreature()->AttackBonus(heavy_weapon), 9);
    EXPECT_NE(light.Source, heavy.Source);
    EXPECT_EQ(Snapshot(), before);
    io_.ExpectTarget("Second");
    rng_.ExpectCall(20, 10);
    rng_.ExpectCall(12, 7);
    EXPECT_EQ(battle_->Execute(heavy), EAbilityExecutionResult::Applied);
    EXPECT_EQ(first.GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 89);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(actor.GetCreature()->AttacksMade().Get(), 1);
    EXPECT_EQ(io_.Candidates(), (std::vector<std::string>{"First", "Second"}));
}

TEST_F(TGeneratedStrikeTest, NpcNaturalProfileUsesFinalAttackAndDamageBaselines)
{
    auto& actor = Add(Natural(), "Wolf", {0, 0}, 0);
    auto& target = Add(Target(), "Target", {1, 0});
    const auto& jaws = *actor.GetCreature()->NaturalWeapons()->Find(TId<TWeapon>{0});
    EXPECT_EQ(actor.GetCreature()->WeaponProficiency(EWeaponCategory::Unarmed), 3);
    EXPECT_EQ(actor.GetCreature()->AttackBonus(jaws), 9);
    io_.ExpectTarget("Target");
    rng_.ExpectCall(20, 6);
    rng_.ExpectCall(6, 3);
    EXPECT_EQ(battle_->Execute(Option(actor, 0, true)), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 95);
    EXPECT_EQ(actor.GetCreature()->AttacksMade().Get(), 1);
}

TEST_F(TGeneratedStrikeTest, LethalStrikeCompletesAndOnlyTheOtherCreatureRemainsTargetable)
{
    auto& actor = Add(Armed(nlohmann::json::array({StrikeWeapon(12)})), "Fighter", {0, 0}, 0);
    auto wounded = Target();
    wounded["pf2e_creature"]["creature_data"]["hitpoints"]["current"] = 5;
    auto& first = Add(std::move(wounded), "First", {1, 0});
    auto& second = Add(Target(), "Second", {0, 1});
    const auto option = Option(actor);
    io_.ExpectTarget("First");
    rng_.ExpectCall(20, 10);
    rng_.ExpectCall(12, 7);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    EXPECT_FALSE(first.GetCreature()->IsAlive());
    EXPECT_EQ(first.GetCreature()->Hitpoints()->Current(), 0);
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 100);

    io_.ExpectTarget("Second");
    rng_.ExpectCall(20, 15);
    rng_.ExpectCall(12, 7);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(io_.Candidates(), (std::vector<std::string>{"Second"}));
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 89);
    EXPECT_EQ(actor.GetCreature()->AttacksMade().Get(), 2);
}

TEST_F(TGeneratedStrikeTest, DegreesOfSuccessNaturalAdjustmentsAndMissesUseOneAttack)
{
    struct TCase { int Natural; int ArmorClass; int Damage; };
    auto& actor = Add(Armed(), "Fighter", {0, 0}, 0);
    auto& target = Add(Target(), "Target", {1, 0});
    const auto option = Option(actor);
    const auto before = Snapshot();
    auto context = journal_->ExecutionContext();
    const auto initial = context.GetState();
    for (const auto test : std::vector<TCase>{
            {6, 15, 7}, {5, 15, 0}, {16, 15, 14}, {1, 15, 0},
            {20, 35, 7}, {20, 40, 0}, {1, 0, 7}}) {
        SCOPED_TRACE("natural=" + std::to_string(test.Natural)
                     + " AC=" + std::to_string(test.ArmorClass));
        context.AssignGuarded(target.GetCreature()->ArmorClassBaseline(),
            TStatisticBaseline::Create({.IsFixed = true, .Value = test.ArmorClass}),
            TGuarded<TCreature>(*target.GetCreature()), "armor_class_baseline");
        io_.ExpectTarget("Target");
        rng_.ExpectCall(20, test.Natural);
        if (test.Damage > 0) {
            rng_.ExpectCall(6, 3);
        }
        EXPECT_EQ(ExecuteJournal(actor, option), EAbilityExecutionResult::Applied);
        EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100 - test.Damage);
        EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
        EXPECT_EQ(actor.GetCreature()->AttacksMade().Get(), 1);
        context.Undo(initial);
        EXPECT_EQ(Snapshot(), before);
    }
}

TEST_F(TGeneratedStrikeTest, FatalReplacesAllBaseDiceAndCriticalOnlyDiceAreNotDoubled)
{
    auto weapon = StrikeWeapon();
    weapon["damage_dice_count"] = 2;
    weapon["traits"] = nlohmann::json::array({
        {{"Fatal", "D10"}},
        {{"Deadly", "D8"}}
    });
    auto& actor = Add(Armed(nlohmann::json::array({weapon})), "Fighter", {0, 0}, 0);
    auto& target = Add(Target(), "Target", {1, 0});
    const auto option = Option(actor);
    const auto initial = journal_->CurrentState();
    io_.ExpectTarget("Target");
    rng_.ExpectCall(20, 10);
    rng_.ExpectCall(6, 3);
    rng_.ExpectCall(6, 5);
    EXPECT_EQ(ExecuteJournal(actor, option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 88);
    journal_->Undo(initial);

    io_.ExpectTarget("Target");
    rng_.ExpectCall(20, 20);
    rng_.ExpectCall(10, 3);
    rng_.ExpectCall(10, 5);
    rng_.ExpectCall(10, 7);
    rng_.ExpectCall(8, 6);
    EXPECT_EQ(ExecuteJournal(actor, option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 63);
    EXPECT_EQ(actor.GetCreature()->AttacksMade().Get(), 1);
}

TEST_F(TGeneratedStrikeTest, MinimumDamageIsAppliedBeforeCriticalDoubling)
{
    auto weapon = StrikeWeapon();
    weapon["damage_modifier_baseline"] = {{"is_fixed", true}, {"value", -8}};
    auto& actor = Add(Armed(nlohmann::json::array({weapon})), "Fighter", {0, 0}, 0);
    auto& target = Add(Target(), "Target", {1, 0});
    io_.ExpectTarget("Target");
    rng_.ExpectCall(20, 20);
    rng_.ExpectCall(6, 1);
    EXPECT_EQ(battle_->Execute(Option(actor)), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 98);
}

TEST_F(TGeneratedStrikeTest, PackAttackNeedsTwoOtherLivingAlliesAndUsesTheirDeclaredReach)
{
    auto& actor = Add(Natural(true), "Wolf A", {0, 0}, 0);
    auto& target = Add(Target(), "Target", {1, 0});
    Add(Natural(), "Wolf B", {1, 1}, 0);
    auto defeated = Natural();
    defeated["pf2e_creature"]["creature_data"]["hitpoints"]["current"] = 0;
    Add(std::move(defeated), "Defeated ally", {2, 0}, 0);
    const auto option = Option(actor, 0, true);
    io_.ExpectTarget("Target");
    rng_.ExpectCall(20, 10);
    rng_.ExpectCall(6, 3);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 95);

    Add(Natural(false, 2), "Long reach ally", {3, 0}, 0);
    io_.ExpectTarget("Target");
    rng_.ExpectCall(20, 15);
    rng_.ExpectCall(6, 3);
    rng_.ExpectCall(4, 2);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 88);
    EXPECT_EQ(actor.GetCreature()->AttacksMade().Get(), 2);
}

TEST_F(TGeneratedStrikeTest, AnAllyCanBeTargetedButIsNotCountedAsAnOtherPackAlly)
{
    auto& actor = Add(Natural(true), "Wolf A", {0, 0}, 0);
    auto target_data = Natural();
    target_data["pf2e_creature"]["creature_data"]["armor_class_baseline"] = {
        {"is_fixed", true}, {"value", 15}};
    auto& target = Add(std::move(target_data), "Friendly target", {1, 0}, 0);
    Add(Natural(), "Other ally", {1, 1}, 0);
    Add(Target(), "Enemy", {5, 5});
    io_.ExpectTarget("Friendly target");
    rng_.ExpectCall(20, 10);
    rng_.ExpectCall(6, 3);
    EXPECT_EQ(battle_->Execute(Option(actor, 0, true)), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 95);
}

TEST_F(TGeneratedStrikeTest, PackDamageIsCombinedBeforeDefensesAndTemporaryHitpoints)
{
    auto& actor = Add(Natural(true), "Wolf A", {0, 0}, 0);
    auto definition = Target(15, 4);
    auto& data = definition["pf2e_creature"]["creature_data"];
    data["resistances"] = {{"Piercing", 5}};
    data["vulnerabilities"] = {{"Piercing", 3}};
    auto& target = Add(std::move(definition), "Target", {1, 0});
    Add(Natural(), "Wolf B", {2, 0}, 0);
    Add(Natural(), "Wolf C", {1, 1}, 0);
    const auto option = Option(actor, 0, true);
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    for (const bool critical : {false, true}) {
        io_.ExpectTarget("Target");
        rng_.ExpectCall(20, critical ? 20 : 10);
        rng_.ExpectCall(6, 3);
        rng_.ExpectCall(4, 2);
        EXPECT_EQ(ExecuteJournal(actor, option), EAbilityExecutionResult::Applied);
        EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), critical ? 92 : 99);
        EXPECT_EQ(target.GetCreature()->Hitpoints()->Temporary(), 0);
        journal_->Undo(initial);
        EXPECT_EQ(Snapshot(), before);
    }
}

TEST_F(TGeneratedStrikeTest, ImmunityAndFullResistancePreserveTemporaryHitpoints)
{
    auto& actor = Add(Armed(), "Fighter", {0, 0}, 0);
    auto immune = Target(15, 5);
    auto& immune_data = immune["pf2e_creature"]["creature_data"];
    immune_data["immunities"] = nlohmann::json::array({"Piercing"});
    immune_data["vulnerabilities"] = {{"Piercing", 100}};
    auto& first = Add(std::move(immune), "Immune", {1, 0});
    auto resistant = Target(15, 5);
    resistant["pf2e_creature"]["creature_data"]["resistances"] = {{"Piercing", 100}};
    auto& second = Add(std::move(resistant), "Resistant", {0, 1});
    const auto option = Option(actor);
    io_.ExpectTarget("Immune");
    rng_.ExpectCall(20, 10);
    rng_.ExpectCall(6, 3);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    io_.ExpectTarget("Resistant");
    rng_.ExpectCall(20, 15);
    rng_.ExpectCall(6, 3);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(first.GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_EQ(first.GetCreature()->Hitpoints()->Temporary(), 5);
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Temporary(), 5);
    EXPECT_EQ(actor.GetCreature()->AttacksMade().Get(), 2);
}

TEST_F(TGeneratedStrikeTest, RangePreflightRejectsStaleOptionsBeforeCostOrAttackCount)
{
    auto& actor = Add(Armed(), "Fighter", {0, 0}, 0);
    auto& target = Add(Target(), "Target", {1, 0});
    const auto option = Option(actor);
    const auto initial = journal_->CurrentState();
    journal_->MovePlayer(&target, {5, 5});
    const auto before = Snapshot();
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(ExecuteJournal(actor, option), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
    EXPECT_EQ(actor.GetCreature()->AttacksMade().Get(), 0);
    EXPECT_EQ(Snapshot(), before);
    journal_->Undo(initial);
    EXPECT_EQ(battle_->AvailableActions(actor).size(), 1u);
}

TEST_F(TGeneratedStrikeTest, SourceRemovalSnapshotAndReplayPreserveSinglePaymentAndAttackCount)
{
    auto& actor = Add(Armed(), "Fighter", {0, 0}, 0);
    auto& target = Add(Target(), "Target", {1, 0});
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectTarget("Target", true);
    rng_.ExpectCall(20, 10);
    rng_.ExpectCall(6, 3);
    const auto point = Suspend([&]() { ExecuteJournal(actor, Option(actor)); });
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(actor.GetCreature()->AttacksMade().Get(), 0);
    TTransformator source_editor(io_);
    auto context = source_editor.ExecutionContext();
    const auto edit_start = context.GetState();
    context.AssignGuarded(actor.GetCreature()->Weapons(), TIdCollection<TWeapon>{},
                          TGuarded<TCreature>(*actor.GetCreature()), "weapons");
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(rng_.RemainingCalls(), 2u);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100);
    context.Undo(edit_start);
    auto weapon = context.Project(actor.GetCreature()->Weapons(), [](TIdCollection<TWeapon>& values) {
        return TGuarded<TWeapon>(*values.Find(TId<TWeapon>{0}));
    });
    auto die = context.Project(weapon, [](TWeapon& value) { return value.BaseDiceSize(); });
    context.AssignGuarded(die, 12, weapon, "base_dice_size");
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 93);
    EXPECT_EQ(actor.GetCreature()->AttacksMade().Get(), 1);
    point.Revert(*journal_);
    rng_.ExpectCall(20, 10);
    rng_.ExpectCall(6, 4);
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 92);
    EXPECT_EQ(actor.GetCreature()->AttacksMade().Get(), 1);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    context.Undo(edit_start);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

}
