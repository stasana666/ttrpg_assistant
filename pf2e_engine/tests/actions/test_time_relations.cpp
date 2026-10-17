#include <pf2e_engine/actions/generated_relation.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/rules.h>
#include <pf2e_engine/inventory/combat_timing.h>

#include "../test_lib/mock_dice_roller.h"
#include "../test_lib/mock_interaction_system.h"

#include <gtest/gtest.h>

#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {

template <class T>
struct TRelationShape {
    using TOwner = TCreature;
    using TKey = TCreature;
    using TValue = T;
    static TAstNode GetAst(const T& value, TAstContext& context)
    {
        auto node = TAstNode::MakeObject("payload");
        if constexpr (TIsAstRecursive<T>::value) {
            AddOwnedObject(node, "value", value, context);
        } else {
            AddValueField(node, "value", value);
        }
        return node;
    }
};

struct TPermission : TRelationShape<bool> {
    static constexpr std::string_view kId = "relations.Permission";
};

struct TProfile : TRelationShape<TWeapon> {
    static constexpr std::string_view kId = "relations.Profile";
};

struct TConflictingPermission : TPermission {};

struct TMissingPermission : TPermission {
    static constexpr std::string_view kId = "relations.Missing";
};

TCreature Participant()
{
    TIdCollection<TFeatureSelection> features;
    features.Add(TFeatureSelection("relations.Training"));
    return TCreature::Create({
        .Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 20}), .Current = 20}),
        .Features = std::move(features), .ZeroHpRule = EZeroHpRule::Death, .Actions = TResource(3),
    });
}

class TTimeRelationsTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        rules_ = std::make_shared<TAbilityRules>();
        rules_->Time.RoundDurationSeconds = CreateCombatTiming().RoundSeconds();
        rules_->Relations.Register<TPermission>();
        rules_->Relations.Register<TProfile>();
        rules_->Catalog.Register({"relations.Probe", EAbilityKind::Action, {}});
        rules_->Catalog.Register({"relations.Training", EAbilityKind::Feature, {"relations.Probe"}});
        rules_->Executions.Register("relations.Probe", std::make_shared<const TAction>(
            [this](auto context) {
                internal_journal_ = context->transformator;
                if (on_action_) {
                    on_action_(std::move(context));
                }
            }, TAction::TBoundResources{}, "Probe"));
        battle_ = std::make_unique<TBattle>(TBattleMap(6, 6), &rng_, io_, rules_);
        journal_ = std::make_unique<TTransformator>(io_);
        for (int id = 0; id < 3; ++id) {
            creatures_.push_back(Participant());
            rng_.ExpectCall(20, 10);
            battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{id == 0 ? 0 : 1}, TPlayerId{id},
                                       "Participant", ""), {id, 0});
        }
    }

    TPlayer& Player(int id)
    {
        return *battle_->GetIfPlayers([id](const auto* player) { return player->GetId() == id; }).at(0);
    }

    TGeneratedEntityBinding<TCreature> Entity(int id) { return BindActionEntity<TCreature>(Player(id)); }
    TGeneratedWorldView World() const { return TGeneratedWorldView(battle_.get()); }

    std::shared_ptr<TActionContext> Context()
    {
        auto context = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(), .dice_roller = &rng_, .transformator = journal_.get(),
            .io_system = &io_,
        });
        context->actor = &Player(0);
        return context;
    }

    void Set(bool value, TGameDuration duration, int owner = 1, int key = 0)
    {
        SetRelation<TPermission>(Context(), Entity(owner), Entity(key).ReadOnly(), value, duration);
    }

    bool Has(int owner = 1, int key = 0)
    {
        return HasRelation<TPermission>(World(), Entity(owner).ReadOnly(), Entity(key).ReadOnly());
    }

    TAstNode Snapshot() const
    {
        TAstContext context;
        auto node = TAstNode::MakeObject("test");
        AddOwnedObject(node, "battle", *battle_, context);
        AddOwnedObject(node, "journal", *journal_, context);
        return node;
    }

    void PrimeInternalJournal()
    {
        ASSERT_EQ(battle_->Execute(battle_->AvailableActions(Player(0)).at(0)), EAbilityExecutionResult::Applied);
        ASSERT_NE(internal_journal_, nullptr);
    }

    void ExpectCompletedRound()
    {
        for (int id = 0; id < 3; ++id) {
            io_.ExpectChoice(id, "next action", "End of turn");
        }
    }

    void TearDown() override
    {
        rng_.Verify();
        io_.Verify();
    }

    TMockRng rng_;
    TMockInteractionSystem io_;
    std::deque<TCreature> creatures_;
    std::shared_ptr<TAbilityRules> rules_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> journal_;
    TTransformator* internal_journal_ = nullptr;
    TAction::FBody on_action_;
};

}

TEST_F(TTimeRelationsTest, OrderedPairsAndFalsePayloadAreIndependentAndReadsArePure)
{
    EXPECT_FALSE(Has());
    Set(false, GameMinutes(10));
    Set(true, GameSeconds(2), 1, 2);
    EXPECT_TRUE(Has());
    EXPECT_TRUE(Has(1, 2));
    EXPECT_FALSE(Has(0, 1));
    EXPECT_FALSE(GetRelation<TPermission>(World(), Entity(1), Entity(0), true));
    const auto before = Snapshot();
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(Has());
        EXPECT_TRUE(GetRelation<TPermission>(World(), Entity(0), Entity(1), true));
    }
    EXPECT_EQ(Snapshot(), before);
    battle_->AdvanceTime(GameSeconds(2), *journal_);
    EXPECT_TRUE(Has());
    EXPECT_FALSE(Has(1, 2));
}

TEST_F(TTimeRelationsTest, ExactExpirationKeepsExpiredStorageAndUndoRestoresDeadline)
{
    Set(true, GameSeconds(10));
    const auto checkpoint = journal_->CurrentState();
    const auto before = Snapshot();
    battle_->AdvanceTime(GameSeconds(9), *journal_);
    EXPECT_TRUE(Has());
    battle_->AdvanceTime(GameSeconds(1), *journal_);
    EXPECT_FALSE(Has());
    const auto expired = Snapshot();
    EXPECT_FALSE(GetRelation<TPermission>(World(), Entity(1), Entity(0), false));
    EXPECT_EQ(Snapshot(), expired);
    EraseRelation<TPermission>(Context(), Entity(1), Entity(0));
    EXPECT_NE(Snapshot(), expired);
    journal_->Undo(checkpoint);
    EXPECT_EQ(Snapshot(), before);
    EXPECT_TRUE(Has());
}

TEST_F(TTimeRelationsTest, PermanentAndTimedReplacementHaveDistinctMeaningAndExactUndo)
{
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    SetRelation<TPermission>(Context(), Entity(1), Entity(0), false);
    battle_->AdvanceTime(GameMinutes(100), *journal_);
    EXPECT_TRUE(Has());
    Set(true, GameSeconds(0));
    EXPECT_FALSE(Has());
    Set(true, GameSeconds(2));
    battle_->AdvanceTime(GameSeconds(1), *journal_);
    SetRelation<TPermission>(Context(), Entity(1), Entity(0), true);
    battle_->AdvanceTime(GameMinutes(100), *journal_);
    EXPECT_TRUE(Has());
    EraseRelation<TPermission>(Context(), Entity(1), Entity(0));
    EXPECT_FALSE(Has());
    const auto erased = Snapshot();
    EraseRelation<TPermission>(Context(), Entity(1), Entity(0));
    EXPECT_EQ(Snapshot(), erased);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TTimeRelationsTest, OrdinaryPayloadIsCopiedAndDoesNotRetainActionContext)
{
    auto context = Context();
    std::weak_ptr<TActionContext> weak = context;
    SetRelation<TProfile>(context, Entity(1), Entity(0),
        TWeapon::Create({.Name = "Stored", .BaseDiceSize = 12}), GameSeconds(5));
    context.reset();
    EXPECT_TRUE(weak.expired());
    auto copy = GetRelation<TProfile>(World(), Entity(1), Entity(0), TWeapon::Create({}));
    EXPECT_EQ(std::as_const(copy).Name(), "Stored");
    copy = TWeapon::Create({.Name = "Changed", .BaseDiceSize = 4});
    const auto original = GetRelation<TProfile>(World(), Entity(1), Entity(0), TWeapon::Create({}));
    EXPECT_EQ(original.Name(), "Stored");
    EXPECT_EQ(original.BaseDiceSize(), 12);
    battle_->AdvanceTime(GameSeconds(5), *journal_);
    const auto fallback = GetRelation<TProfile>(World(), Entity(1), Entity(0), std::move(copy));
    EXPECT_EQ(fallback.Name(), "Changed");
}

TEST_F(TTimeRelationsTest, InvalidDurationClockAndDeadlineOverflowLeaveJournalUntouched)
{
    const auto initial = Snapshot();
    EXPECT_THROW(Set(true, GameSeconds(-1)), std::invalid_argument);
    EXPECT_THROW(GameMinutes(-1), std::invalid_argument);
    battle_->AdvanceTime(GameSeconds(0), *journal_);
    EXPECT_EQ(Snapshot(), initial);
    EXPECT_EQ(GameMinutes(std::numeric_limits<int>::max()).Seconds(),
              static_cast<std::uint64_t>(std::numeric_limits<int>::max()) * 60);
    battle_->AdvanceTime(TGameDuration::FromSeconds(std::numeric_limits<std::uint64_t>::max()), *journal_);
    const auto full = Snapshot();
    EXPECT_THROW(battle_->AdvanceTime(GameSeconds(1), *journal_), std::overflow_error);
    EXPECT_THROW(Set(true, GameSeconds(1)), std::overflow_error);
    EXPECT_EQ(Snapshot(), full);
    SetRelation<TPermission>(Context(), Entity(1), Entity(0), true);
    EXPECT_TRUE(Has());
    Set(false, GameSeconds(0));
    EXPECT_FALSE(Has());
}

TEST_F(TTimeRelationsTest, ReadonlyOwnerForeignWorldAndInvalidContextCannotWrite)
{
    const auto before = Snapshot();
    EXPECT_THROW(SetRelation<TPermission>(Context(), Entity(1).ReadOnly(), Entity(0), true), std::logic_error);
    EXPECT_THROW(EraseRelation<TPermission>(Context(), Entity(1).ReadOnly(), Entity(0)), std::logic_error);
    EXPECT_THROW(SetRelation<TPermission>(nullptr, Entity(1), Entity(0), true), std::invalid_argument);
    EXPECT_THROW(HasRelation<TPermission>(TGeneratedWorldView{}, Entity(1), Entity(0)), std::invalid_argument);
    TBattle foreign(TBattleMap(3, 3), &rng_, io_, rules_);
    EXPECT_THROW(HasRelation<TPermission>(TGeneratedWorldView(&foreign), Entity(1), Entity(0)), TAbilityUnavailable);
    auto context = Context();
    context->battle = &foreign;
    EXPECT_THROW(SetRelation<TPermission>(context, Entity(1), Entity(0), true), TAbilityUnavailable);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TTimeRelationsTest, SourceDeathAndFeatureRemovalDoNotCancelItsRelation)
{
    Set(true, GameMinutes(10), 1, 2);
    auto context = journal_->ExecutionContext();
    context.AssignGuarded(Player(2).GetCreature()->Features(), TIdCollection<TFeatureSelection>{},
                           TGuarded<TCreature>(*Player(2).GetCreature()), "features");
    journal_->DealDamage(&Player(2), 20);
    ASSERT_TRUE(Player(2).GetCreature()->IsDead());
    EXPECT_TRUE(Has(1, 2));
    EXPECT_TRUE(GetRelation<TPermission>(World(), Entity(1).ReadOnly(), Entity(2).ReadOnly(), false));
}

TEST_F(TTimeRelationsTest, RegistryRejectsConflictingTagsAndUnknownWritesBeforeMutation)
{
    EXPECT_NO_THROW(rules_->Relations.Register<TPermission>());
    EXPECT_THROW(rules_->Relations.Register<TConflictingPermission>(), std::invalid_argument);
    const auto before = Snapshot();
    EXPECT_THROW(SetRelation<TMissingPermission>(Context(), Entity(1), Entity(0), true), std::invalid_argument);
    EXPECT_THROW(GetRelation<TMissingPermission>(World(), Entity(1), Entity(0), false), std::invalid_argument);
    EXPECT_THROW(EraseRelation<TMissingPermission>(Context(), Entity(1), Entity(0)), std::invalid_argument);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TTimeRelationsTest, NewEncounterStartsWithEmptyClockAndRelations)
{
    Set(true, GameMinutes(10));
    battle_->AdvanceTime(GameSeconds(5), *journal_);
    auto owner_creature = TCreature::Create({});
    auto key_creature = TCreature::Create({});
    TBattle other(TBattleMap(3, 3), &rng_, io_, rules_);
    rng_.ExpectCall(20, 10);
    other.AddPlayer(TPlayer(&key_creature, TPlayerTeam{0}, TPlayerId{0}, "Key", ""), {0, 0});
    rng_.ExpectCall(20, 10);
    other.AddPlayer(TPlayer(&owner_creature, TPlayerTeam{1}, TPlayerId{1}, "Owner", ""), {1, 0});
    const auto players = other.GetIfPlayers([](const auto*) { return true; });
    const auto key = BindActionEntity<TCreature>(*players.at(0));
    const auto owner = BindActionEntity<TCreature>(*players.at(1));
    EXPECT_EQ(other.ElapsedSeconds(), 0);
    EXPECT_FALSE(HasRelation<TPermission>(TGeneratedWorldView(&other), owner, key));
    EXPECT_TRUE(Has());
    const auto address = MakeRelationAddress(World(), TPermission::kId, Entity(1).View(), Entity(0).View());
    const auto foreign_address = MakeRelationAddress(TGeneratedWorldView(&other), TPermission::kId,
                                                     owner.View(), key.View());
    EXPECT_EQ(battle_->Relations().Find(foreign_address, 0), nullptr);
    EXPECT_NE(battle_->Relations().Find(address, 0), nullptr);
}

TEST_F(TTimeRelationsTest, SuspendedWriteUsesCurrentClockAndReplaysThroughItsJournalCheckpoint)
{
    auto context = Context();
    const auto target = Entity(1);
    const auto source = Entity(0).ReadOnly();
    std::optional<TSavepointStackUnwind> pending;
    try {
        throw TSavepointStackUnwind(journal_->CurrentState(), [context, target, source] {
            SetRelation<TPermission>(context, target, source, true, GameSeconds(2));
        });
    } catch (const TSavepointStackUnwind& savepoint) {
        pending = savepoint;
    }
    battle_->AdvanceTime(GameSeconds(3), *journal_);
    pending->Resume();
    battle_->AdvanceTime(GameSeconds(1), *journal_);
    EXPECT_TRUE(Has());
    battle_->AdvanceTime(GameSeconds(1), *journal_);
    EXPECT_FALSE(Has());
    pending->Revert(*journal_);
    EXPECT_EQ(battle_->ElapsedSeconds(), 0);
    EXPECT_FALSE(Has());
    pending->Resume();
    const auto replay = Snapshot();
    pending->Revert(*journal_);
    pending->Resume();
    EXPECT_EQ(Snapshot(), replay);
}

TEST_F(TTimeRelationsTest, IndependentBattlesNormalizeRelationAndClockAst)
{
    Set(false, GameSeconds(10));
    battle_->AdvanceTime(GameSeconds(3), *journal_);
    std::deque<TCreature> creatures;
    TBattle other(TBattleMap(6, 6), &rng_, io_, rules_);
    TTransformator other_journal(io_);
    for (int id = 0; id < 3; ++id) {
        creatures.push_back(Participant());
        ASSERT_NE(creatures.back().Id(), Entity(id).Read().Id());
        rng_.ExpectCall(20, 10);
        other.AddPlayer(TPlayer(&creatures.back(), TPlayerTeam{id == 0 ? 0 : 1}, TPlayerId{id},
                                "Participant", ""), {id, 0});
    }
    const auto players = other.GetIfPlayers([](const auto*) { return true; });
    auto context = Context();
    context->battle = &other;
    context->transformator = &other_journal;
    context->actor = players.at(0);
    SetRelation<TPermission>(context, BindActionEntity<TCreature>(*players.at(1)),
                              BindActionEntity<TCreature>(*players.at(0)), false, GameSeconds(10));
    other.AdvanceTime(GameSeconds(3), other_journal);
    TAstContext ast_context;
    auto node = TAstNode::MakeObject("test");
    AddOwnedObject(node, "battle", other, ast_context);
    AddOwnedObject(node, "journal", other_journal, ast_context);
    EXPECT_EQ(node, Snapshot());
}

TEST_F(TTimeRelationsTest, CompletedRoundAdvancesOnceAndExactBattleReplayRestoresTime)
{
    on_action_ = [this](const auto& context) {
        SetRelation<TPermission>(context, Entity(1), Entity(0), true,
                                  GameSeconds(rules_->Time.RoundDurationSeconds));
    };
    PrimeInternalJournal();
    const auto checkpoint = internal_journal_->CurrentState();
    const auto before = Snapshot();
    for (int replay = 0; replay < 2; ++replay) {
        for (int id = 0; id < 3; ++id) {
            io_.AddCheckCallback([this] {
                EXPECT_EQ(battle_->ElapsedSeconds(), 0);
                EXPECT_TRUE(Has());
            });
            io_.ExpectChoice(id, "next action", "End of turn");
        }
        io_.AddCheckCallback([this] {
            EXPECT_EQ(battle_->ElapsedSeconds(), 6);
            EXPECT_FALSE(Has());
        });
        EXPECT_THROW(battle_->StartBattle(), TTooManyCallsError);
        EXPECT_EQ(battle_->ElapsedSeconds(), 6);
        internal_journal_->Undo(checkpoint);
        EXPECT_EQ(Snapshot(), before);
    }
}

TEST_F(TTimeRelationsTest, PartialRoundVictoryDoesNotAdvanceClock)
{
    on_action_ = [this](const auto& context) {
        context->transformator->DealDamage(&Player(1), 20);
        context->transformator->DealDamage(&Player(2), 20);
    };
    io_.ExpectChoice(0, "next action", "Probe");
    EXPECT_NO_THROW(battle_->StartBattle());
    EXPECT_EQ(battle_->ElapsedSeconds(), 0);
    EXPECT_EQ(battle_->Winner(), 0);
}

TEST(GameTimeRulesTest, FactoryUsesAuthoredRoundDurationAndNativeDefaultContainsNoSystemConstant)
{
    EXPECT_EQ(TGameTimeRules{}.RoundDurationSeconds, 0);
    const auto timing = CreateCombatTiming();
    EXPECT_EQ(timing.RoundSeconds(), 6);
    EXPECT_EQ(CreateAbilityRules()->Time.RoundDurationSeconds, timing.RoundSeconds());
    auto invalid = std::make_shared<TAbilityRules>();
    invalid->Time.RoundDurationSeconds = -1;
    TMockRng rng;
    TMockInteractionSystem io;
    EXPECT_THROW(TBattle(TBattleMap(3, 3), &rng, io, invalid), std::invalid_argument);
    rng.Verify();
}
