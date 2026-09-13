#include <pf2e_engine/inventory/rule_entities.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/transformation/transformator.h>

#include <mock_interaction_system.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {

static_assert(!std::is_copy_constructible_v<TTestCombatant>);
static_assert(!std::is_copy_assignable_v<TTestCombatant>);
static_assert(std::is_move_constructible_v<TTestCombatant>);
static_assert(std::is_move_assignable_v<TTestCombatant>);

TAstNode Snapshot(const TTestCombatant& actor, const TTransformator& transformator)
{
    TAstContext ctx;
    TAstNode result = TAstNode::MakeObject("generated_rules");
    AddOwnedObject(result, "journal", transformator, ctx);
    AddOwnedObject(result, "actor", actor, ctx);
    return result;
}

}

TEST(RuleEntityTest, NamedFactoriesCreateIndependentIdentitiesAndInitialValues)
{
    const TTestCombatant first = CreateWolf();
    const TTestCombatant second = CreateWolf();
    const TTestCombatant fighter = CreateFighter();

    EXPECT_NE(first.Id().Value, 0u);
    EXPECT_NE(first.Id(), second.Id());
    EXPECT_NE(first.Id(), fighter.Id());
    EXPECT_EQ(first.Name(), "Wolf");
    EXPECT_EQ(first.Level(), 1);
    EXPECT_EQ(first.Hitpoints().Current(), 24);
    EXPECT_EQ(first.Hitpoints().Maximum().Value(), 24);
    EXPECT_EQ(fighter.Name(), "Fighter");
    EXPECT_EQ(fighter.Hitpoints().Current(), 21);
}

TEST(RuleEntityTest, MoveTransfersIdentityAndClearsSourceIdentity)
{
    TTestCombatant source = CreateWolf();
    const auto identity = source.Id();
    TTestCombatant target(std::move(source));
    EXPECT_EQ(target.Id(), identity);
    EXPECT_EQ(source.Id().Value, 0u);

    TTestCombatant replacement = CreateFighter();
    replacement = std::move(target);
    EXPECT_EQ(replacement.Id(), identity);
    EXPECT_EQ(target.Id().Value, 0u);
    EXPECT_EQ(std::as_const(replacement).Name(), "Wolf");
}

TEST(RuleEntityTest, NestedLegacyClassesRunComputedInitializers)
{
    const TTestCombatant actor = CreateWolf();

    EXPECT_EQ(actor.LegacyValue().Base(), 5);
    EXPECT_EQ(actor.LegacyValue().Doubled(), 10);
}

TEST(RuleEntityTest, NestedMethodsMutateOnlyTheirInstanceAndUndoTogether)
{
    TMockInteractionSystem io;
    TTransformator transformator(io);
    auto ctx = transformator.ExecutionContext();
    TTestCombatant first = CreateWolf();
    const TTestCombatant second = CreateWolf();
    const auto initial = ctx.GetState();
    const auto before = Snapshot(first, transformator);

    first.SetTemporary(ctx, 5);
    const TDamageResult result = first.TakeDamage(ctx, 8);
    EXPECT_EQ(result.AbsorbedTemporary(), 5);
    EXPECT_EQ(result.LostCurrent(), 3);
    EXPECT_EQ(std::as_const(first).Hitpoints().Current(), 21);
    EXPECT_EQ(std::as_const(first).Hitpoints().Temporary(), 0);
    EXPECT_EQ(std::as_const(first).ReceivedDamage(), 3);
    EXPECT_EQ(second.Hitpoints().Current(), 24);

    const std::string changed = Snapshot(first, transformator).PrettyPrint();
    EXPECT_NE(changed.find("ref:entity#" + std::to_string(first.Id().Value) + ".hitpoints"),
              std::string::npos);
    ctx.Undo(initial);
    EXPECT_EQ(std::as_const(first).Hitpoints().Current(), 24);
    EXPECT_EQ(Snapshot(first, transformator), before);
}

TEST(RuleEntityTest, OuterFailureRollsBackCommittedNestedMethods)
{
    TMockInteractionSystem io;
    TTransformator transformator(io);
    auto ctx = transformator.ExecutionContext();
    TTestCombatant actor = CreateFighter();
    actor.SetTemporary(ctx, 2);
    actor.TakeDamage(ctx, 3);
    const auto before = Snapshot(actor, transformator);

    EXPECT_THROW(actor.TakeDamageAndFail(ctx, 7), std::runtime_error);

    EXPECT_EQ(std::as_const(actor).Hitpoints().Current(), 20);
    EXPECT_EQ(std::as_const(actor).ReceivedDamage(), 1);
    EXPECT_EQ(Snapshot(actor, transformator), before);
}

TEST(RuleEntityTest, ConditionalQueriesDoNotChangeStateOrJournal)
{
    TMockInteractionSystem io;
    TTransformator transformator(io);
    auto ctx = transformator.ExecutionContext();
    TTestCombatant actor = CreateFighter();
    const auto full = Snapshot(actor, transformator);
    EXPECT_EQ(actor.MissingHitpoints(), 0);
    EXPECT_EQ(Snapshot(actor, transformator), full);

    actor.TakeDamage(ctx, 6);
    const auto wounded = Snapshot(actor, transformator);
    EXPECT_EQ(actor.MissingHitpoints(), 6);
    EXPECT_EQ(Snapshot(actor, transformator), wounded);
    EXPECT_EQ(actor.Heal(ctx, 99), 6);
    EXPECT_EQ(actor.MissingHitpoints(), 0);
}

TEST(RuleEntityTest, ArithmeticOverflowRollsBackEarlierNestedChanges)
{
    TMockInteractionSystem io;
    TTransformator transformator(io);
    auto ctx = transformator.ExecutionContext();
    TTestCombatant actor = CreateFighter();
    const auto before = Snapshot(actor, transformator);

    EXPECT_THROW(actor.TakeDamageThenOverflow(ctx, 1), std::overflow_error);

    EXPECT_EQ(std::as_const(actor).Hitpoints().Current(), 21);
    EXPECT_EQ(std::as_const(actor).ReceivedDamage(), 0);
    EXPECT_EQ(Snapshot(actor, transformator), before);
}

TEST(RuleEntityTest, NestedInitializersUseMaximumAndRespectExplicitCurrent)
{
    TGameObjectFactory factory;
    const TTestCombatant computed = TTestCombatant::FromJson(
        {{"hitpoints", {{"maximum", {{"base", 31}}}}}}, factory);
    EXPECT_EQ(computed.Hitpoints().Current(), 31);

    const TTestCombatant explicit_current = TTestCombatant::FromJson(
        {{"hitpoints", {{"maximum", {{"base", 31}}}, {"current", 4}}}}, factory);
    EXPECT_EQ(explicit_current.Hitpoints().Maximum().Value(), 31);
    EXPECT_EQ(explicit_current.Hitpoints().Current(), 4);
}

TEST(RuleEntityTest, RestorePreservesIdentityAndWoundsWithoutReinitialization)
{
    TMockInteractionSystem io;
    TTransformator transformator(io);
    auto ctx = transformator.ExecutionContext();
    TTestCombatant actor = CreateFighter();
    actor.TakeDamage(ctx, 9);
    actor.SetTemporary(ctx, 3);
    TGameObjectFactory factory;
    const auto snapshot = actor.ToJson();

    const TTestCombatant restored = TTestCombatant::Restore(snapshot, factory);
    EXPECT_EQ(restored.Id(), actor.Id());
    EXPECT_EQ(restored.Hitpoints().Current(), 12);
    EXPECT_EQ(restored.Hitpoints().Temporary(), 3);
    EXPECT_EQ(restored.ReceivedDamage(), 9);
    EXPECT_EQ(restored.ToJson(), snapshot);

    auto incomplete = snapshot;
    incomplete["hitpoints"].erase("current");
    EXPECT_THROW(TTestCombatant::Restore(incomplete, factory), nlohmann::json::out_of_range);
}

TEST(RuleEntityTest, RestoreRejectsInvalidIdentity)
{
    const TTestCombatant actor = CreateWolf();
    TGameObjectFactory factory;
    for (const nlohmann::json& invalid : {nlohmann::json(-1), nlohmann::json(0),
                                         nlohmann::json(1.5), nlohmann::json("7")}) {
        auto snapshot = actor.ToJson();
        snapshot["id"] = invalid;
        EXPECT_THROW(TTestCombatant::Restore(snapshot, factory), std::invalid_argument);
    }
}

TEST(RuleEntityTest, CollectionSnapshotsPreserveGapsAndNextIdentity)
{
    TIdCollection<Statistic> values;
    const auto removed = values.Add(Statistic::Create({.Base = 2}));
    const auto retained = values.Add(Statistic::Create({.Base = 7}));
    const auto removed_last = values.Add(Statistic::Create({.Base = 11}));
    values.Remove(removed);
    values.Remove(removed_last);
    const auto next = values.NextId();
    const TTestStatisticCollection collection = TTestStatisticCollection::Create({.Items = values});
    TGameObjectFactory factory;
    const auto snapshot = collection.ToJson();

    const TTestStatisticCollection restored = TTestStatisticCollection::Restore(snapshot, factory);
    EXPECT_EQ(restored.Items().NextId(), next);
    EXPECT_EQ(restored.Items().Size(), 1u);
    ASSERT_NE(restored.Items().Find(retained), nullptr);
    EXPECT_EQ(restored.Items().Find(retained)->Value(), 7);
    EXPECT_EQ(restored.ToJson(), snapshot);

    auto invalid = snapshot;
    invalid["items"]["entries"][0]["id"] = -1;
    EXPECT_THROW(TTestStatisticCollection::Restore(invalid, factory), std::invalid_argument);
    invalid = snapshot;
    invalid["items"]["next_id"] = -1;
    EXPECT_THROW(TTestStatisticCollection::Restore(invalid, factory), std::invalid_argument);
}

TEST(RuleEntityTest, CollectionAstIncludesAllocatorState)
{
    TIdCollection<Statistic> first_values;
    first_values.Add(Statistic::Create({.Base = 5}));
    auto second_values = first_values;
    const auto removed = second_values.Add(Statistic::Create({.Base = 6}));
    second_values.Remove(removed);
    const TTestStatisticCollection first = TTestStatisticCollection::Create({.Items = first_values});
    const TTestStatisticCollection second = TTestStatisticCollection::Create({.Items = second_values});
    TAstContext first_ctx;
    TAstContext second_ctx;

    EXPECT_NE(first.GetAst(first_ctx), second.GetAst(second_ctx));
}
