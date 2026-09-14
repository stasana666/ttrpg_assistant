#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/inventory/generated_records.h>

#include "../test_lib/mock_dice_roller.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <any>
#include <deque>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

class TRecordIo final : public IInteractionSystem {
public:
    void ExpectName(int actor, std::string kind, std::string name, bool suspend = false,
                    std::function<void(const TAlternatives&)> observe = {})
    {
        expected_.push_back({actor, std::move(kind),
            [name = std::move(name)](const TAlternative& alternative) {
                return alternative.name == name;
            }, suspend, std::move(observe)});
    }

    template <class T>
    void ExpectValue(int actor, T value, bool suspend = false,
                     std::function<void(const TAlternatives&)> observe = {})
    {
        expected_.push_back({actor, {}, [value](const TAlternative& alternative) {
            const auto* actual = std::any_cast<T>(&alternative.value);
            return actual != nullptr && *actual == value;
        }, suspend, std::move(observe)});
    }

    void RequestChoice(int actor, TAlternatives alternatives, TState state,
                       std::function<void(size_t)> receive) override
    {
        if (expected_.empty()) {
            throw std::logic_error("unexpected record choice: " + std::string(alternatives.Kind()));
        }
        auto expected = std::move(expected_.front());
        expected_.pop_front();
        EXPECT_EQ(actor, expected.Actor);
        if (!expected.Kind.empty()) {
            EXPECT_EQ(alternatives.Kind(), expected.Kind);
        }
        size_t index = 0;
        while (index < alternatives.Size() && !expected.Matches(alternatives[index])) {
            ++index;
        }
        if (index == alternatives.Size()) {
            throw std::logic_error("missing record alternative");
        }
        if (expected.Observe) {
            expected.Observe(alternatives);
        }
        ++requests_;
        if (expected.Suspend) {
            throw TSavepointStackUnwind(state, [receive = std::move(receive), index]() {
                receive(index);
            });
        }
        receive(index);
    }

    void HandleReactionTrigger(const TTriggerContext&, const TState&) override { ++legacy_calls_; }
    std::ostream& GameLog() override { return log_; }
    std::ostream& DevLog() override { return log_; }
    size_t Requests() const { return requests_; }
    int LegacyCalls() const { return legacy_calls_; }
    void Verify() const { EXPECT_TRUE(expected_.empty()); }

protected:
    size_t ChooseAlternativeIndex(int, const TAlternatives&) override
    {
        throw std::logic_error("unexpected legacy choice");
    }

private:
    struct TExpected {
        int Actor;
        std::string Kind;
        std::function<bool(const TAlternative&)> Matches;
        bool Suspend;
        std::function<void(const TAlternatives&)> Observe;
    };
    std::deque<TExpected> expected_;
    std::ostringstream log_;
    size_t requests_ = 0;
    int legacy_calls_ = 0;
};

class TGeneratedRecordsTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedRecordsRules(*rules);
        RegisterGeneratedRecordsRules(*rules);
        nlohmann::json map = {{"x_size", 5}, {"y_size", 5}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, std::move(rules));
        journal_ = std::make_unique<TTransformator>(io_);
    }

    void TearDown() override
    {
        rng_.Verify();
        io_.Verify();
        EXPECT_EQ(io_.LegacyCalls(), 0);
    }

    static TIdCollection<TFeatureSelection> Features(std::initializer_list<std::string> names)
    {
        TIdCollection<TFeatureSelection> result;
        for (const auto& name : names) {
            result.Add(TFeatureSelection(name));
        }
        return result;
    }

    static TWeapon Weapon(int size = 4)
    {
        return TWeapon::Create({.Name = "Blade", .BaseDiceSize = size,
            .Features = Features({"generated_records.WeaponTraining"})});
    }

    TPlayer& Add(bool armed = false)
    {
        const int id = static_cast<int>(creatures_.size());
        TIdCollection<TWeapon> weapons;
        if (armed) {
            weapons.Add(Weapon());
            weapons.Add(Weapon(8));
        }
        creatures_.push_back(TCreature::Create({
            .Name = "Actor " + std::to_string(id),
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = 100}), .Current = 100,
            }),
            .Weapons = std::move(weapons),
            .Features = Features({"generated_records.RootTraining"}),
            .Actions = TResource(10), .Reactions = TResource(3),
        }));
        rng_.ExpectCall(20, 10);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{id}, TPlayerId{id},
            "Actor " + std::to_string(id), ""), {id, 0});
        return Player(id);
    }

    TPlayer& Player(int id)
    {
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    TCreature& Creature(int id) { return *Player(id).GetCreature(); }
    const TCreature& Read(int id) { return Creature(id); }

    std::vector<TAbilityOption> Options(const std::string& name, int actor = 0)
    {
        auto result = battle_->AvailableActions(Player(actor));
        std::erase_if(result, [&](const TAbilityOption& option) {
            return option.DefinitionId != "generated_records." + name;
        });
        return result;
    }

    TAbilityOption Option(const std::string& name, int actor = 0, size_t source = 0)
    {
        return Options(name, actor).at(source);
    }

    EAbilityExecutionResult Execute(const TAbilityOption& option, int actor = 0)
    {
        const auto& rules = battle_->AbilityRules();
        auto context = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(), .dice_roller = &rng_, .transformator = journal_.get(),
            .io_system = &io_, .scheduler = nullptr, .effect_manager = nullptr,
        });
        return ExecuteAbility(option, context, Player(actor),
            CollectAbilities(Read(actor), rules.Catalog), rules.Executions,
            {.ValidateSource = [this, actor, definition = option.DefinitionId, source = option.Source]() {
                const auto fresh = CollectAbilities(Read(actor), battle_->AbilityRules().Catalog);
                return std::ranges::any_of(fresh.Grants(), [&](const TGrantedAbility& grant) {
                    return grant.DefinitionId == definition && grant.Source == source;
                });
            }});
    }

    EAbilityExecutionResult Execute(const std::string& name, int actor = 0, size_t source = 0)
    {
        return Execute(Option(name, actor, source), actor);
    }

    void Attack(int target = 1, bool suspend = false)
    {
        io_.ExpectName(0, "target", "Actor " + std::to_string(target));
        io_.ExpectValue(0, 1, suspend);
    }

    bool HasRecord(int actor = 0)
    {
        return battle_->History().Find(Player(actor), TRecordHitResult::kId).has_value();
    }

    static TSavepointStackUnwind Pause(const std::function<void()>& work)
    {
        try {
            work();
        } catch (const TSavepointStackUnwind& point) {
            return point;
        }
        throw std::logic_error("expected a record suspension");
    }

    TAstNode Snapshot()
    {
        TAstContext context;
        auto node = TAstNode::MakeObject("generated_records_test");
        AddOwnedObject(node, "battle", *battle_, context);
        AddOwnedObject(node, "journal", *journal_, context);
        return node;
    }

    static void SetDie(TTransformator& editor, TCreature& creature, int amount)
    {
        auto execution = editor.ExecutionContext();
        const auto weapon = execution.Project(creature.Weapons(), [](TIdCollection<TWeapon>& values) {
            return TGuarded<TWeapon>(*values.Find(TId<TWeapon>{0}));
        });
        const auto die = execution.Project(weapon, [](TWeapon& value) { return value.BaseDiceSize(); });
        execution.AssignGuarded(die, amount, weapon, "base_dice_size");
    }

    TRecordIo io_;
    TMockRng rng_;
    std::deque<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> journal_;
};

TEST_F(TGeneratedRecordsTest, TerminalPublicationWaitsForChildCompletionAndFollowConsumesTheRecord)
{
    Add(true);
    Add();
    EXPECT_TRUE(Options("Follow").empty());
    Attack(1, true);
    const auto point = Pause([&] { Execute("BoundAttack"); });
    EXPECT_EQ(Read(0).Actions().Count(), 9);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 100);
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("Follow").empty());
    point.Resume();
    EXPECT_TRUE(HasRecord());
    EXPECT_EQ(Read(1).Hitpoints().Current(), 95);
    const auto follows = Options("Follow");
    ASSERT_EQ(follows.size(), 1U);
    EXPECT_TRUE(follows[0].Source.StartsWith(TAbilitySourcePath{}.Field("weapons").Entry(0)));
    io_.ExpectValue(0, 2);
    io_.ExpectValue(0, 1);
    EXPECT_EQ(Execute(follows[0]), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(0).Actions().Count(), 8);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 88);
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("Follow").empty());
}

TEST_F(TGeneratedRecordsTest, SelectedSourceAliasPublishesTheExactSecondSameNamedWeapon)
{
    Add(true);
    Add();
    io_.ExpectName(0, "target", "Actor 1");
    io_.ExpectName(0, "source", "Blade (2)");
    io_.ExpectValue(0, 1);
    EXPECT_EQ(Execute("SelectAttack"), EAbilityExecutionResult::Applied);
    const auto follows = Options("Follow");
    ASSERT_EQ(follows.size(), 1U);
    EXPECT_TRUE(follows[0].Source.StartsWith(TAbilitySourcePath{}.Field("weapons").Entry(1)));
    EXPECT_EQ(Read(1).Hitpoints().Current(), 91);
    EXPECT_EQ(Read(0).Actions().Count(), 9);
}

TEST_F(TGeneratedRecordsTest, SavedFollowOptionCannotRetargetAfterANewerSuccessfulAttack)
{
    Add(true);
    Add();
    Add();
    Attack();
    Execute("BoundAttack");
    const auto stale = Option("Follow");
    Attack(2);
    Execute("BoundAttack");
    const auto before = Snapshot();
    EXPECT_EQ(Execute(stale), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(Snapshot(), before);
    EXPECT_EQ(Read(0).Actions().Count(), 8);
    EXPECT_EQ(Execute("HistoricalFollow"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 95);
    EXPECT_EQ(Read(2).Hitpoints().Current(), 91);
}

TEST_F(TGeneratedRecordsTest, RejectedPreflightPreservesHistoryButPaidFailureAndAnotherActionInvalidateIt)
{
    Add(true);
    Add();
    Attack();
    Execute("BoundAttack");
    const auto published = journal_->CurrentState();
    const auto before = Snapshot();
    EXPECT_TRUE(Options("Unavailable").empty());
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    editor.ExecutionContext().AssignGuarded(Creature(0).Level(), 6,
        TGuarded<TCreature>(Creature(0)), "level");
    const auto stale = Option("Unavailable");
    editor.Undo(edit);
    EXPECT_EQ(Execute(stale), EAbilityExecutionResult::Unavailable);
    EXPECT_TRUE(HasRecord());
    EXPECT_EQ(Snapshot(), before);
    EXPECT_THROW(Execute("Fail"), std::runtime_error);
    EXPECT_FALSE(HasRecord());
    EXPECT_EQ(Read(0).Actions().Count(), 8);
    journal_->Undo(published);
    EXPECT_EQ(Snapshot(), before);
    Execute("Wait");
    EXPECT_FALSE(HasRecord());
    journal_->Undo(published);
    io_.ExpectName(0, "target", "Actor 1");
    Execute("Miss");
    EXPECT_TRUE(HasRecord());
    EXPECT_TRUE(Options("Follow").empty());
}

TEST_F(TGeneratedRecordsTest, OtherActorsActionsAndReactionsLeaveThePublishersRecordAvailable)
{
    Add(true);
    Add();
    Attack();
    Execute("BoundAttack");
    Execute("Wait", 1);
    EXPECT_EQ(Options("Follow").size(), 1U);
    io_.ExpectName(1, "target", "Actor 1");
    io_.ExpectName(1, "reaction", "Intervene");
    Execute("OpenWindow", 1);
    EXPECT_EQ(Read(1).Reactions().Count(), 2);
    EXPECT_EQ(Options("Follow").size(), 1U);
}

TEST_F(TGeneratedRecordsTest, OwnReactionInvalidatesHistoryAndLateParentPublicationCannotReplaceIt)
{
    Add(true);
    Add();
    Attack();
    Execute("BoundAttack");
    io_.ExpectName(1, "target", "Actor 0");
    io_.ExpectName(0, "reaction", "Intervene");
    Execute("OpenWindow", 1);
    EXPECT_FALSE(HasRecord());
    io_.ExpectName(0, "target", "Actor 1");
    io_.ExpectName(0, "reaction", "Intervene");
    EXPECT_EQ(Execute("PublishAfterWindow"), EAbilityExecutionResult::Applied);
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("Follow").empty());
    EXPECT_EQ(Read(0).Reactions().Count(), 1);
}

TEST_F(TGeneratedRecordsTest, AcceptedFollowRetainsInputsThroughTwoSuspensionsAndExactReplay)
{
    Add(true);
    Add();
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    Attack();
    Execute("BoundAttack");
    io_.ExpectValue(0, 2, true);
    const auto first = Pause([&] { Execute("Follow"); });
    EXPECT_FALSE(HasRecord());
    EXPECT_EQ(Read(0).Actions().Count(), 8);
    io_.ExpectValue(0, 1, true);
    const auto second = Pause([&] { first.Resume(); });
    second.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 88);
    const auto completed = Snapshot();
    second.Revert(*journal_);
    second.Resume();
    EXPECT_EQ(Snapshot(), completed);
    first.Revert(*journal_);
    io_.ExpectValue(0, 1);
    first.Resume();
    EXPECT_EQ(Snapshot(), completed);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedRecordsTest, RemovingAndReplacingTheSourceCannotSatisfyTheRecordedIdentity)
{
    Add(true);
    Add();
    Attack();
    Execute("BoundAttack");
    const auto option = Option("Follow");
    TTransformator editor(io_);
    const auto initial = editor.CurrentState();
    auto weapons = Read(0).Weapons();
    weapons.Remove(TId<TWeapon>{0});
    weapons.Add(Weapon());
    editor.ExecutionContext().AssignGuarded(Creature(0).Weapons(), std::move(weapons),
        TGuarded<TCreature>(Creature(0)), "weapons");
    EXPECT_TRUE(Options("Follow").empty());
    EXPECT_TRUE(Options("HistoricalFollow").empty());
    EXPECT_EQ(Execute(option), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(Read(0).Actions().Count(), 9);
    editor.Undo(initial);
    EXPECT_EQ(Options("Follow").size(), 1U);
    EXPECT_EQ(Options("HistoricalFollow").size(), 1U);
}

TEST_F(TGeneratedRecordsTest, HistoricalSourceSnapshotSurvivesAttributeChangesAcrossAChildPause)
{
    Add(true);
    Add();
    Attack();
    Execute("BoundAttack");
    io_.ExpectValue(0, 2);
    io_.ExpectValue(0, 1, true);
    const auto point = Pause([&] { Execute("Follow"); });
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    SetDie(editor, Creature(0), 12);
    point.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 88);
    EXPECT_EQ(Read(0).Weapons().Find(TId<TWeapon>{0})->BaseDiceSize(), 12);
    editor.Undo(edit);
}

TEST_F(TGeneratedRecordsTest, RemovingHistoricalSourceDuringAChildPauseRejectsEffectsUntilRestored)
{
    Add(true);
    Add();
    Attack();
    Execute("BoundAttack");
    io_.ExpectValue(0, 2);
    io_.ExpectValue(0, 1, true);
    const auto point = Pause([&] { Execute("Follow"); });
    TTransformator editor(io_);
    const auto edit = editor.CurrentState();
    auto weapons = Read(0).Weapons();
    weapons.Remove(TId<TWeapon>{0});
    editor.ExecutionContext().AssignGuarded(Creature(0).Weapons(), std::move(weapons),
        TGuarded<TCreature>(Creature(0)), "weapons");
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 95);
    EXPECT_EQ(Read(0).Actions().Count(), 8);
    editor.Undo(edit);
    point.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 88);
    const auto completed = Snapshot();
    point.Revert(*journal_);
    point.Resume();
    EXPECT_EQ(Snapshot(), completed);
}

TEST_F(TGeneratedRecordsTest, HistoricalInputDoesNotDependOnTheOldAttackGrant)
{
    Add(true);
    Add();
    Attack();
    Execute("BoundAttack");
    auto execution = journal_->ExecutionContext();
    auto weapon = execution.Project(Creature(0).Weapons(), [](TIdCollection<TWeapon>& values) {
        return TGuarded<TWeapon>(*values.Find(TId<TWeapon>{0}));
    });
    auto features = execution.Project(weapon, [](TWeapon& value) { return value.Features(); });
    execution.AssignGuarded(features, TIdCollection<TFeatureSelection>{}, weapon, "features");
    EXPECT_EQ(Options("BoundAttack").size(), 1U);
    EXPECT_TRUE(Options("Follow").empty());
    EXPECT_EQ(Execute("HistoricalFollow"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 91);
}

TEST_F(TGeneratedRecordsTest, SuccessfulSubordinateActivityDoesNotPublishARootActionRecord)
{
    Add(true);
    Add();
    Attack();
    EXPECT_EQ(Execute("Subordinate"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(Read(1).Hitpoints().Current(), 95);
    EXPECT_FALSE(HasRecord());
    EXPECT_TRUE(Options("Follow").empty());
    EXPECT_EQ(Read(0).Actions().Count(), 9);
}

}
