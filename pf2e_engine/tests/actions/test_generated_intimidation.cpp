#include "../test_lib/battle_fixture.h"

#include <pf2e_engine/action_blocks/add_condition.h>
#include <pf2e_engine/action_blocks/remove_condition.h>
#include <pf2e_engine/actions/generated_relation.h>
#include <pf2e_engine/inventory/remaster_profiles.h>

#include <array>

namespace {

class TGeneratedIntimidationTest : public TGeneratedCombatFixture {
protected:
    void Pair(TPosition target = {1, 0})
    {
        Join(CreateHumanFighter1(), 0, {0, 0});
        Join(CreateWolf(), 1, target);
        BeginTurn();
    }

    bool Immune(int target = 1, int actor = 0)
    {
        return HasRelation<TRelationDemoralizeImmunity>(TGeneratedWorldView(battle_.get()),
            BindActionEntity<TCreature>(Player(target)), BindActionEntity<TCreature>(Player(actor)));
    }

    template <class TTag>
    void Relation(int owner, int key, bool value, std::shared_ptr<TActionContext> context = {})
    {
        SetRelation<TTag>(context ? context : Context(), BindActionEntity<TCreature>(Player(owner)),
            BindActionEntity<TCreature>(Player(key)), value);
    }

    void Attempt(int natural, bool glare = true, int actor = 0, int target = 1, bool suspend = false)
    {
        io_.ExpectName(actor, "target", std::string(Player(target).GetName()), suspend);
        rng_.ExpectCall(20, natural);
        Execute(glare ? "intimidation.UseIntimidatingGlare" : "intimidation.UseDemoralize", actor);
    }
};

TEST_F(TGeneratedIntimidationTest, OrdinaryDemoralizeAppliesLanguagePenaltyAndBothModesShareImmunity)
{
    Pair();
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    Attempt(12, false);
    EXPECT_EQ(Read(1).ConditionPenalty(), 0);
    EXPECT_TRUE(Immune());
    EXPECT_FALSE(Available("intimidation.UseDemoralize"));
    EXPECT_FALSE(Available("intimidation.UseIntimidatingGlare"));
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
    Attempt(12);
    EXPECT_EQ(Read(1).ConditionPenalty(), 1);
    EXPECT_TRUE(Immune());
    EXPECT_EQ(Read(0).AttacksMade(), 0);
}

TEST_F(TGeneratedIntimidationTest, AllFourOutcomesGrantImmunityWithoutApplyingOrIncreasingMap)
{
    Pair();
    const auto start = journal_->CurrentState();
    for (const auto& [natural, expected] : std::array<std::pair<int, int>, 4>{{{1, 0}, {2, 0}, {12, 1}, {20, 2}}}) {
        auto execution = journal_->ExecutionContext();
        execution.Invoke(TGuarded<TCreature>(Creature(0)), &TCreature::RecordAttack, 2);
        Attempt(natural);
        EXPECT_EQ(Read(1).ConditionPenalty(), expected);
        EXPECT_TRUE(Immune());
        EXPECT_EQ(Read(0).AttacksMade(), 2);
        EXPECT_EQ(Read(0).Actions().Count(), 2);
        journal_->Undo(start);
    }
}

TEST_F(TGeneratedIntimidationTest, SharedSpokenLanguageRemovesPenaltyButSignLanguageDoesNot)
{
    Join(CreateHumanFighter1(), 0, {0, 0});
    auto target = CreateHumanFighter1({.Name = "Target"});
    Join(std::move(target), 1, {1, 0});
    BeginTurn();
    const auto initial = journal_->CurrentState();
    Attempt(11, false);
    EXPECT_EQ(Read(1).ConditionPenalty(), 1);
    journal_->Undo(initial);
    auto execution = journal_->ExecutionContext();
    TIdCollection<TLanguage> languages;
    languages.Add(TLanguage::Create({.Name = "Common", .Spoken = false}));
    execution.AssignGuarded(Creature(1).Languages(), languages, TGuarded<TCreature>(Creature(1)), "languages");
    Attempt(11, false);
    EXPECT_EQ(Read(1).ConditionPenalty(), 0);
}

TEST_F(TGeneratedIntimidationTest, RangeUsesThirtyFeetAndOrdinaryDiagonalDistance)
{
    const auto rules = TIntimidationRules::Create({});
    EXPECT_TRUE(rules.InRange(0, 0, 6, 0));
    EXPECT_FALSE(rules.InRange(0, 0, 7, 0));
    EXPECT_TRUE(rules.InRange(0, 0, 4, 4));
    EXPECT_FALSE(rules.InRange(0, 0, 5, 4));
    Pair({7, 0});
    EXPECT_FALSE(Available("intimidation.UseDemoralize"));
    EXPECT_FALSE(Available("intimidation.UseIntimidatingGlare"));
    EXPECT_EQ(Read(0).Actions().Count(), 3);
    EXPECT_EQ(io_.Requests(), 0U);
}

TEST_F(TGeneratedIntimidationTest, ObservationOverridesAreDirectionalAndSeparateSoundFromSight)
{
    Pair();
    Relation<TRelationVisibility>(0, 1, false);
    EXPECT_TRUE(Available("intimidation.UseIntimidatingGlare"));
    Relation<TRelationVisibility>(1, 0, false);
    EXPECT_FALSE(Available("intimidation.UseIntimidatingGlare"));
    EXPECT_TRUE(Available("intimidation.UseDemoralize"));
    Relation<TRelationAudibility>(1, 0, false);
    EXPECT_FALSE(Available("intimidation.UseDemoralize"));
    Relation<TRelationVisibility>(1, 0, true);
    EXPECT_TRUE(Available("intimidation.UseIntimidatingGlare"));
    Relation<TRelationAwareness>(0, 1, false);
    EXPECT_FALSE(Available("intimidation.UseIntimidatingGlare"));
    EXPECT_EQ(Read(0).Actions().Count(), 3);
}

TEST_F(TGeneratedIntimidationTest, InabilityToVocalizeLeavesVisualModeAvailable)
{
    Pair();
    auto execution = journal_->ExecutionContext();
    execution.AssignGuarded(Creature(0).CanVocalize(), false,
        TGuarded<TCreature>(Creature(0)), "can_vocalize");
    EXPECT_FALSE(Available("intimidation.UseDemoralize"));
    EXPECT_TRUE(Available("intimidation.UseIntimidatingGlare"));
    execution.Invoke(TGuarded<TCreature>(Creature(1)), &TCreature::BecomeUnconscious);
    EXPECT_FALSE(Available("intimidation.UseIntimidatingGlare"));
}

TEST_F(TGeneratedIntimidationTest, FearFromDifferentActorsUsesMaximumAndExpiresOnlyAtTargetsTurnEnd)
{
    Pair();
    Join(CreateHumanFighter1({.Name = "Second fighter"}), 0, {0, 1});
    BeginTurn(2);
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    Attempt(20);
    EXPECT_TRUE(Immune(1, 0));
    EXPECT_FALSE(Immune(1, 2));
    Attempt(10, true, 2);
    EXPECT_TRUE(Immune(1, 2));
    EXPECT_EQ(Read(1).ConditionPenalty(), 2);
    BeginTurn(1);
    EXPECT_EQ(Read(1).ConditionPenalty(), 2);
    auto execution = journal_->ExecutionContext();
    execution.Invoke(TGuarded<TCreature>(Creature(0)), &TCreature::EndTurn);
    EXPECT_EQ(Read(1).ConditionPenalty(), 2);
    execution.Invoke(TGuarded<TCreature>(Creature(1)), &TCreature::EndTurn);
    EXPECT_EQ(Read(1).ConditionPenalty(), 1);
    execution.Invoke(TGuarded<TCreature>(Creature(1)), &TCreature::EndTurn);
    EXPECT_EQ(Read(1).ConditionPenalty(), 0);
    EXPECT_TRUE(Immune(1, 0));
    EXPECT_TRUE(Immune(1, 2));
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedIntimidationTest, TwoWolfTargetsHaveIndependentImmunityAndTemporaryImmunityExpiresAtTenMinutes)
{
    Pair();
    Join(CreateWolf({.Name = "Second wolf"}), 1, {1, 1});
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    Attempt(2);
    EXPECT_TRUE(Immune(1));
    EXPECT_FALSE(Immune(2));
    Attempt(12, true, 0, 2);
    EXPECT_TRUE(Immune(2));
    battle_->AdvanceTime(GameSeconds(599), *journal_);
    EXPECT_TRUE(Immune(1));
    EXPECT_TRUE(Immune(2));
    const auto unexpired = journal_->CurrentState();
    battle_->AdvanceTime(GameSeconds(1), *journal_);
    EXPECT_FALSE(Immune(1));
    EXPECT_FALSE(Immune(2));
    journal_->Undo(unexpired);
    EXPECT_TRUE(Immune(1));
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedIntimidationTest, SuspendedSelectionRechecksObservationBeforeRollAndReplaysExactly)
{
    Pair();
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    io_.ExpectName(0, "target", "Wolf", true);
    const auto point = Pause([&] { Execute("intimidation.UseIntimidatingGlare"); });
    EXPECT_EQ(Read(0).Actions().Count(), 2);
    TTransformator editor(io_);
    auto context = Context();
    context->transformator = &editor;
    const auto edit = editor.CurrentState();
    Relation<TRelationVisibility>(1, 0, false, context);
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_FALSE(Immune());
    EXPECT_EQ(Read(1).ConditionPenalty(), 0);
    editor.Undo(edit);
    rng_.ExpectCall(20, 12);
    point.Resume();
    const auto complete = Snapshot();
    point.Revert(*journal_);
    rng_.ExpectCall(20, 12);
    point.Resume();
    EXPECT_EQ(Snapshot(), complete);
    EXPECT_TRUE(Immune());
    EXPECT_EQ(Read(1).ConditionPenalty(), 1);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedIntimidationTest, SuspendedSelectionRejectsNewImmunityBeforeRolling)
{
    Pair();
    io_.ExpectName(0, "target", "Wolf", true);
    const auto point = Pause([&] { Execute("intimidation.UseIntimidatingGlare"); });
    Relation<TRelationDemoralizeImmunity>(1, 0, true);
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Read(1).ConditionPenalty(), 0);
    EXPECT_EQ(Read(0).Actions().Count(), 2);
}

TEST_F(TGeneratedIntimidationTest, LegacyConditionAdaptersUseDeclaredFearWithoutSchedulerState)
{
    Pair();
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    const auto id = [](const std::string& name) { return TGameObjectIdManager::Instance().Register(name); };
    auto context = Context();
    context->game_object_registry->Add(id("target"), &Player(1));
    for (int amount : {2, 1}) {
        TBlockInput input;
        input.Add(id("target"), id("target"));
        input.Add(id("condition"), std::string("Frightened"));
        input.Add(id("value"), amount);
        FAddCondition(std::move(input), id("void"))(context);
    }
    EXPECT_EQ(Read(1).ConditionPenalty(), 2);
    BeginTurn(1);
    EXPECT_EQ(Read(1).ConditionPenalty(), 2);
    auto execution = journal_->ExecutionContext();
    execution.Invoke(TGuarded<TCreature>(Creature(1)), &TCreature::EndTurn);
    EXPECT_EQ(Read(1).ConditionPenalty(), 1);
    TBlockInput input;
    input.Add(id("target"), id("target"));
    input.Add(id("condition"), std::string("Frightened"));
    FRemoveCondition(std::move(input), id("void"))(context);
    EXPECT_EQ(Read(1).ConditionPenalty(), 0);
    EXPECT_EQ(context->scheduler, nullptr);
    EXPECT_EQ(context->effect_manager, nullptr);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}


TEST_F(TGeneratedIntimidationTest, DroppingAShieldFreesItsHandOnceThroughKnockoutHealingAndUndo)
{
    Pair();
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    auto execution = journal_->ExecutionContext();
    EXPECT_EQ(Read(0).Hands().Count(), 0);
    execution.Invoke(TGuarded<TCreature>(Creature(0)), &TCreature::ApplyResolvedDamage,
        22, TDamageContext::Create({}));
    EXPECT_FALSE(Read(0).Shield().Wielded());
    EXPECT_EQ(Read(0).Hands().Count(), 1);
    execution.Invoke(TGuarded<TCreature>(Creature(0)), &TCreature::DropShield);
    EXPECT_EQ(Read(0).Hands().Count(), 1);
    execution.Invoke(TGuarded<TCreature>(Creature(0)), &TCreature::ReceiveHealing, 5);
    EXPECT_TRUE(Available("trip.UseTrip"));
    EXPECT_EQ(Read(0).Hands().Count(), 1);
    execution.Invoke(TGuarded<TCreature>(Creature(0)), &TCreature::Die);
    EXPECT_EQ(Read(0).Hands().Count(), 1);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}


}
