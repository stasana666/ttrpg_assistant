#include "../test_lib/battle_fixture.h"

#include <pf2e_engine/actions/generated_relation.h>
#include <pf2e_engine/inventory/remaster_profiles.h>

#include <array>

namespace {

class TGeneratedIntimidationTest : public TGeneratedCombatFixture {
protected:
    void Pair(TPosition target = {1, 0})
    {
        Join(CreateHumanFighter1(), 0, {0, 0}, "Fighter");
        Join(CreateWolf(), 1, target);
        BeginTurn();
    }

    bool Immune(int target = 1, int actor = 0)
    {
        return HasRelation<TRelationDemoralizeImmunity>(TGeneratedWorldView(battle_.get()),
            BindActionEntity<TCreature>(Player(target)), BindActionEntity<TCreature>(Player(actor)));
    }

    template <class TTag>
    void Relation(int owner, int key, bool value)
    {
        SetRelation<TTag>(Context(), BindActionEntity<TCreature>(Player(owner)), BindActionEntity<TCreature>(Player(key)),
            value);
    }

    void Attempt(int natural, bool glare = true, int actor = 0, int target = 1)
    {
        io_.ExpectName(actor, "target", std::string(Player(target).GetName()));
        rng_.ExpectCall(20, natural);
        Execute(glare ? "intimidation.UseIntimidatingGlare" : "intimidation.UseDemoralize", actor);
    }

    void EndTurn(int actor)
    {
        journal_->ExecutionContext().Invoke(TGuarded<TCreature>(Creature(actor)), &TCreature::EndTurn);
    }
};

TEST_F(TGeneratedIntimidationTest, DemoralizeTakesTheLanguagePenaltyAndBothModesShareImmunity)
{
    Pair();
    const auto start = journal_->CurrentState();
    Act("intimidation.UseDemoralize", 0, {.Choose = {"Wolf"}, .Rolls = {{20, 12}},
        .Changes = {{"Fighter.actions.count", 2}}});
    EXPECT_TRUE(Immune());
    EXPECT_FALSE(Available("intimidation.UseDemoralize"));
    EXPECT_FALSE(Available("intimidation.UseIntimidatingGlare"));
    journal_->Undo(start);
    Act("intimidation.UseIntimidatingGlare", 0, {.Choose = {"Wolf"}, .Rolls = {{20, 12}}, .Changes = {
        {"Fighter.actions.count", 2}, {"Wolf.conditions.Frightened", true}, {"Wolf.conditions.Frightened.value", 1}}});
    EXPECT_TRUE(Immune());
}

TEST_F(TGeneratedIntimidationTest, AllFourOutcomesGrantImmunityWithoutTheMultipleAttackPenalty)
{
    Pair();
    journal_->ExecutionContext().Invoke(TGuarded<TCreature>(Creature(0)), &TCreature::RecordAttack, 2);
    const auto start = journal_->CurrentState();
    for (const auto& [natural, fear] : std::array<std::pair<int, int>, 4>{{{1, 0}, {2, 0}, {12, 1}, {20, 2}}}) {
        SCOPED_TRACE(natural);
        TStep step{.Choose = {"Wolf"}, .Rolls = {{20, natural}}, .Changes = {{"Fighter.actions.count", 2}}};
        if (fear > 0) {
            step.Changes.emplace("Wolf.conditions.Frightened", true);
            step.Changes.emplace("Wolf.conditions.Frightened.value", fear);
        }
        Act("intimidation.UseIntimidatingGlare", 0, step);
        EXPECT_TRUE(Immune());
        journal_->Undo(start);
    }
}

TEST_F(TGeneratedIntimidationTest, SharedSpokenLanguageRemovesThePenaltyButSignLanguageDoesNot)
{
    Join(CreateHumanFighter1(), 0, {0, 0});
    Join(CreateHumanFighter1({.Name = "Target"}), 1, {1, 0});
    BeginTurn();
    const auto start = journal_->CurrentState();
    Attempt(11, false);
    EXPECT_EQ(Read(1).ConditionPenalty(), 1);
    journal_->Undo(start);
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
}

TEST_F(TGeneratedIntimidationTest, InabilityToVocalizeLeavesTheGlareAvailable)
{
    Pair();
    auto execution = journal_->ExecutionContext();
    execution.AssignGuarded(Creature(0).CanVocalize(), false, TGuarded<TCreature>(Creature(0)), "can_vocalize");
    EXPECT_FALSE(Available("intimidation.UseDemoralize"));
    EXPECT_TRUE(Available("intimidation.UseIntimidatingGlare"));
    execution.Invoke(TGuarded<TCreature>(Creature(1)), &TCreature::BecomeUnconscious);
    EXPECT_FALSE(Available("intimidation.UseIntimidatingGlare"));
}

TEST_F(TGeneratedIntimidationTest, FearFromTwoActorsKeepsTheMaximumAndDecreasesOnlyAtTheTargetsTurnEnd)
{
    Pair();
    Join(CreateHumanFighter1({.Name = "Second fighter"}), 0, {0, 1});
    BeginTurn(2);
    Attempt(20);
    EXPECT_FALSE(Immune(1, 2));
    Attempt(10, true, 2);
    EXPECT_TRUE(Immune(1, 2));
    EXPECT_EQ(Read(1).ConditionPenalty(), 2);
    BeginTurn(1);
    EndTurn(0);
    EXPECT_EQ(Read(1).ConditionPenalty(), 2);
    EndTurn(1);
    EXPECT_EQ(Read(1).ConditionPenalty(), 1);
    EndTurn(1);
    EXPECT_EQ(Read(1).ConditionPenalty(), 0);
    EXPECT_TRUE(Immune(1, 0));
}

TEST_F(TGeneratedIntimidationTest, EachTargetHasItsOwnImmunityThatExpiresAfterTenMinutes)
{
    Pair();
    Join(CreateWolf({.Name = "Second wolf"}), 1, {1, 1});
    Attempt(2);
    EXPECT_FALSE(Immune(2));
    Attempt(12, true, 0, 2);
    battle_->AdvanceTime(GameSeconds(599), *journal_);
    EXPECT_TRUE(Immune(1));
    EXPECT_TRUE(Immune(2));
    battle_->AdvanceTime(GameSeconds(1), *journal_);
    EXPECT_FALSE(Immune(1));
    EXPECT_FALSE(Immune(2));
}

TEST_F(TGeneratedIntimidationTest, DroppingAShieldFreesItsHandOnlyOnce)
{
    Pair();
    auto execution = journal_->ExecutionContext();
    EXPECT_EQ(Read(0).Hands().Count(), 0);
    execution.Invoke(TGuarded<TCreature>(Creature(0)), &TCreature::ApplyResolvedDamage, 22, TDamageContext::Create({}));
    EXPECT_FALSE(Read(0).Shield().Wielded());
    EXPECT_EQ(Read(0).Hands().Count(), 1);
    execution.Invoke(TGuarded<TCreature>(Creature(0)), &TCreature::DropShield);
    execution.Invoke(TGuarded<TCreature>(Creature(0)), &TCreature::ReceiveHealing, 5);
    EXPECT_TRUE(Available("trip.UseTrip"));
    execution.Invoke(TGuarded<TCreature>(Creature(0)), &TCreature::Die);
    EXPECT_EQ(Read(0).Hands().Count(), 1);
}

}
