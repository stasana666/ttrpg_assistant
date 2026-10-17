#include "../test_lib/battle_fixture.h"

#include <pf2e_engine/actions/feature_event.h>
#include <pf2e_engine/actions/generated_relation.h>
#include <pf2e_engine/inventory/generated_relations.h>

namespace {

class TGeneratedRelationsTest : public TGeneratedCombatFixture {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedRelationsRules(*rules);
        RegisterGeneratedRelationsRules(*rules);
        rules_ = std::move(rules);
        battle_ = std::make_unique<TBattle>(TBattleMap(5, 5), &rng_, io_, rules_);
        journal_ = std::make_unique<TTransformator>(io_);
        Join(Participant(0, false), 0, {0, 0});
        Join(Participant(1, true), 1, {1, 0});
    }

    static TCreature Participant(int id, bool reactor)
    {
        TIdCollection<TFeatureSelection> features;
        features.Add(TFeatureSelection("generated_relations.RelationTraining"));
        if (reactor) {
            features.Add(TFeatureSelection("generated_relations.RelationReactor"));
        }
        TIdCollection<TWeapon> weapons;
        weapons.Add(TWeapon::Create({.Name = "Blade", .BaseDiceSize = 4}));
        weapons.Add(TWeapon::Create({.Name = "Blade", .BaseDiceSize = 8}));
        return TCreature::Create({
            .Name = "Actor " + std::to_string(id), .Level = 5,
            .Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 20}), .Current = 20}),
            .Weapons = std::move(weapons), .Features = std::move(features),
            .Actions = TResource(10), .Reactions = TResource(2),
        });
    }

    TGeneratedEntityBinding<TCreature> Entity(int id)
    {
        return BindActionEntity<TCreature>(Player(id));
    }

    TGeneratedWorldView World() const { return TGeneratedWorldView(battle_.get()); }

    template <class TTag>
    typename TTag::TValue Get(int owner = 0, int key = 1)
    {
        return GetRelation<TTag>(World(), Entity(owner), Entity(key), typename TTag::TValue{});
    }

    bool HasMemory(int owner = 0, int key = 1)
    {
        return HasRelation<TRelationFixtureMemory>(World(), Entity(owner), Entity(key));
    }

    void Capture()
    {
        io_.ExpectName(0, "target", "Actor 1");
        io_.ExpectValue(0, 2);
        ASSERT_EQ(Execute("generated_relations.CaptureRelation"), EAbilityExecutionResult::Applied);
    }
};

TEST_F(TGeneratedRelationsTest, ClassPositionStringEnumAndDurationValuesAreCapturedWhenWritten)
{
    io_.ExpectName(0, "target", "Actor 1", true);
    const auto first = Pause([&] { Execute("generated_relations.CaptureRelation"); });
    auto execution = journal_->ExecutionContext();
    execution.AssignGuarded(Creature(0).Level(), 9, TGuarded<TCreature>(Creature(0)), "level");
    execution.AssignGuarded(Creature(0).Name(), std::string("Changed"), TGuarded<TCreature>(Creature(0)), "name");
    io_.ExpectValue(0, 2, true);
    const auto second = Pause([&] { first.Resume(); });
    EXPECT_FALSE(HasMemory());
    second.Resume();
    const auto payload = Get<TRelationFixtureMemory>();
    EXPECT_EQ(payload.Amount(), 7);
    EXPECT_EQ(payload.Label(), "Actor 0");
    EXPECT_EQ(Get<TRelationFixtureOrigin>(), (TPosition{0, 0}));
    EXPECT_EQ(Get<TRelationFixtureCaption>(), "Actor 0");
    EXPECT_EQ(Get<TRelationFixtureGrade>(), EFixtureRelationGrade::High);
    EXPECT_TRUE(HasRelation<TRelationFixturePresence>(World(), Entity(0), Entity(1)));
    EXPECT_FALSE(Get<TRelationFixturePresence>());
    battle_->AdvanceTime(GameSeconds(4), *journal_);
    EXPECT_TRUE(HasMemory());
    battle_->AdvanceTime(GameSeconds(1), *journal_);
    EXPECT_FALSE(HasMemory());
    EXPECT_EQ(Get<TRelationFixtureCaption>(), "Actor 0");
}

TEST_F(TGeneratedRelationsTest, GeneratedHookReadsTheCurrentPairAndTheExpiredFallbackWithoutMutation)
{
    Capture();
    const auto before = Snapshot();
    const auto evaluate = [&](int owner, int source) {
        return DispatchFeatureEvent<TEventFixtureRelationBonus>(*rules_, World(), Entity(owner),
            TEventFixtureRelationBonus::TArguments{Entity(source)});
    };
    const auto first = evaluate(0, 1);
    ASSERT_EQ(first.size(), 1U);
    EXPECT_EQ(first.front().Value, 7);
    const auto reversed = evaluate(1, 0);
    ASSERT_EQ(reversed.size(), 1U);
    EXPECT_EQ(reversed.front().Value, 0);
    ExpectSame(before, Snapshot(), "event dispatch");
    EXPECT_ANY_THROW(DispatchFeatureEvent<TEventFixtureRelationBonus>(*rules_, Entity(0),
        TEventFixtureRelationBonus::TArguments{Entity(1)}));
    battle_->AdvanceTime(GameSeconds(5), *journal_);
    const auto later = evaluate(0, 1);
    ASSERT_EQ(later.size(), 1U);
    EXPECT_EQ(later.front().Value, 0);
}

TEST_F(TGeneratedRelationsTest, RelationValueCopiesStayStableWhenStorageChangesDuringSuspension)
{
    Capture();
    io_.ExpectName(0, "target", "Actor 1");
    io_.ExpectValue(0, 2, true);
    const auto point = Pause([&] { Execute("generated_relations.ReadRelationSnapshot"); });
    SetRelation<TRelationFixtureMemory>(Context(), Entity(0), Entity(1),
        TFixtureRelationPayload::Create({.Amount = 99, .Label = "replacement"}));
    SetRelation<TRelationFixtureOrigin>(Context(), Entity(0), Entity(1), TPosition{3, 4});
    SetRelation<TRelationFixtureCaption>(Context(), Entity(0), Entity(1), std::string("replacement"));
    point.Resume();
    EXPECT_EQ(Read(0).Hands().Count(), 9);
    EXPECT_EQ(Get<TRelationFixtureMemory>().Amount(), 99);
}

TEST_F(TGeneratedRelationsTest, AYieldedChildReadsRelationsThroughASelectedSource)
{
    Capture();
    Act("generated_relations.UseRelationSource", 0, {.Choose = {"Actor 1", "Blade (2)", TPick::Value(2)},
        .Changes = {{"Actor 0.hands.count", 17}, {"Actor 0.actions.count", 8}}});
}

TEST_F(TGeneratedRelationsTest, AYieldedReactionWritesTheReactorsPair)
{
    Act("generated_relations.OpenRelationWindow", 0, {.Choose = {TPick(1, "RememberRelationReaction"), TPick::Value(4, 1)},
        .Changes = {{"Actor 0.hands.count", 14}, {"Actor 0.actions.count", 9}, {"Actor 1.reactions.count", 1}}});
    EXPECT_FALSE(HasMemory(0, 1));
    EXPECT_EQ(Get<TRelationFixtureMemory>(1, 0).Amount(), 4);
    EXPECT_EQ(Get<TRelationFixtureCaption>(1, 0), "reaction");
    EXPECT_EQ(Get<TRelationFixtureOrigin>(1, 0), (TPosition{1, 0}));
    battle_->AdvanceTime(GameSeconds(3), *journal_);
    EXPECT_FALSE(HasMemory(1, 0));
    EXPECT_EQ(Get<TRelationFixtureCaption>(1, 0), "reaction");
}

}
