#include <pf2e_engine/combat_calculator.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/inventory/proficiency.h>

#include "../ast/ast_test_fixture.h"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <utility>

namespace {

TProficiencyValue MakeFixed(int value)
{
    return TProficiencyValue::Create({.IsFixed = true, .FixedBonus = value});
}

const TWeapon& FirstWeapon(const TCreature& creature)
{
    return **creature.Weapons().begin();
}

}

class ProficiencyCalculationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fixture_ = MakeTwoWarriorBattle();
        transformator_ = std::make_unique<TTransformator>(fixture_->io);
    }

    TCreature& Creature()
    {
        return fixture_->creatures.front();
    }

    TAstNode Snapshot() const
    {
        TAstContext ctx;
        TAstNode node = TAstNode::MakeObject("proficiency_execution");
        AddOwnedObject(node, "journal", *transformator_, ctx);
        AddOwnedObject(node, "battle", *fixture_->battle, ctx);
        return node;
    }

    std::unique_ptr<TTestBattle> fixture_;
    std::unique_ptr<TTransformator> transformator_;
    TCombatCalculator calculator_;
};

TEST_F(ProficiencyCalculationTest, ExistingWarriorBonusesRemainUnchanged)
{
    const auto& creature = Creature();

    EXPECT_EQ(calculator_.AttackRollBonus(creature, FirstWeapon(creature)), 9);
    EXPECT_EQ(calculator_.ArmorClass(creature, creature), 14);
    EXPECT_EQ(calculator_.RollBonus(creature, ESavethrow::Fortitude), 8);
    EXPECT_EQ(calculator_.RollBonus(creature, ESavethrow::Reflex), 6);
    EXPECT_EQ(calculator_.RollBonus(creature, ESavethrow::Will), 4);
    EXPECT_EQ(calculator_.RollBonus(creature, ESkill::Athletics), 7);
    EXPECT_EQ(calculator_.RollBonus(creature, ESkill::Arcana), 1);
    EXPECT_EQ(calculator_.RollBonus(creature, TPerceptionTag{}), 6);
    EXPECT_EQ(calculator_.DifficultyClass(creature, ESavethrow::Will), 14);
    EXPECT_EQ(calculator_.DifficultyClass(creature, ESkill::Athletics), 17);
    EXPECT_EQ(calculator_.DifficultyClass(creature, TPerceptionTag{}), 16);
    EXPECT_EQ(calculator_.InitiativeBonus(creature), 6);
}

TEST_F(ProficiencyCalculationTest, ExistingWolfBonusesAndFinesseSelectionRemainUnchanged)
{
    const auto wolf_id = TGameObjectIdManager::Instance().Register("wolf");
    const auto dagger_id = TGameObjectIdManager::Instance().Register("dagger");
    const auto wolf = fixture_->factory.Create<TCreature>(wolf_id);
    const auto dagger = fixture_->factory.Create<TWeapon>(dagger_id);
    const auto& jaws = **wolf.NaturalWeapons().begin();

    EXPECT_EQ(calculator_.AttackRollBonus(wolf, jaws), 5);
    EXPECT_EQ(calculator_.AttackRollBonus(wolf, dagger), 7);
    EXPECT_EQ(calculator_.ArmorClass(wolf, Creature()), 17);
    EXPECT_EQ(calculator_.RollBonus(wolf, ESavethrow::Fortitude), 7);
    EXPECT_EQ(calculator_.RollBonus(wolf, ESavethrow::Reflex), 9);
    EXPECT_EQ(calculator_.RollBonus(wolf, ESavethrow::Will), 5);
    EXPECT_EQ(calculator_.RollBonus(wolf, ESkill::Athletics), 5);
    EXPECT_EQ(calculator_.RollBonus(wolf, TPerceptionTag{}), 7);
    EXPECT_EQ(calculator_.InitiativeBonus(wolf), 7);
}

TEST_F(ProficiencyCalculationTest, RankedBonusesReadCurrentCreatureLevelAndRollback)
{
    auto ctx = transformator_->ExecutionContext();
    auto& mutable_creature = Creature();
    const auto& creature = mutable_creature;
    const auto before = Snapshot();
    const auto checkpoint = ctx.GetState();

    ctx.AssignGuarded(mutable_creature.Level(), 4, TGuarded<TCreature>(mutable_creature), "level");

    EXPECT_EQ(creature.Level(), 4);
    EXPECT_EQ(calculator_.AttackRollBonus(creature, FirstWeapon(creature)), 12);
    EXPECT_EQ(calculator_.ArmorClass(creature, creature), 17);
    EXPECT_EQ(calculator_.RollBonus(creature, ESavethrow::Fortitude), 11);
    EXPECT_EQ(calculator_.RollBonus(creature, ESavethrow::Reflex), 9);
    EXPECT_EQ(calculator_.RollBonus(creature, ESavethrow::Will), 7);
    EXPECT_EQ(calculator_.RollBonus(creature, ESkill::Athletics), 10);
    EXPECT_EQ(calculator_.RollBonus(creature, TPerceptionTag{}), 9);
    EXPECT_EQ(calculator_.RollBonus(creature, ESkill::Arcana), 1);
    EXPECT_EQ(calculator_.InitiativeBonus(creature), 9);

    ctx.Undo(checkpoint);

    EXPECT_EQ(creature.Level(), 1);
    EXPECT_EQ(calculator_.AttackRollBonus(creature, FirstWeapon(creature)), 9);
    EXPECT_EQ(calculator_.ArmorClass(creature, creature), 14);
    EXPECT_EQ(calculator_.RollBonus(creature, TPerceptionTag{}), 6);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(ProficiencyCalculationTest, FixedContributionsAddAbilityOnceAndDoNotScaleWithLevel)
{
    const auto profile = TProficiency::Create({
        .ArmorCategory = std::map<EArmorCategory, TProficiencyValue>{
            {EArmorCategory::Unarmored, MakeFixed(-2)}},
        .WeaponCategory = std::map<EWeaponCategory, TProficiencyValue>{
            {EWeaponCategory::Martial, MakeFixed(7)}},
        .Savethrow = std::map<ESavethrow, TProficiencyValue>{
            {ESavethrow::Fortitude, MakeFixed(5)},
            {ESavethrow::Reflex, MakeFixed(0)},
            {ESavethrow::Will, MakeFixed(-3)}},
        .Skill = std::map<ESkill, TProficiencyValue>{
            {ESkill::Athletics, MakeFixed(-4)}},
        .Perception = MakeFixed(11),
    });
    auto ctx = transformator_->ExecutionContext();
    auto& mutable_creature = Creature();
    const auto& creature = mutable_creature;
    ctx.AssignGuarded(mutable_creature.Proficiency(), profile,
                      TGuarded<TCreature>(mutable_creature), "proficiency");
    const auto checkpoint = ctx.GetState();
    const auto before = Snapshot();

    for (const int level : {1, 10}) {
        ctx.AssignGuarded(mutable_creature.Level(), level,
                          TGuarded<TCreature>(mutable_creature), "level");
        EXPECT_EQ(calculator_.AttackRollBonus(creature, FirstWeapon(creature)), 11);
        EXPECT_EQ(calculator_.ArmorClass(creature, creature), 9);
        EXPECT_EQ(calculator_.RollBonus(creature, ESavethrow::Fortitude), 8);
        EXPECT_EQ(calculator_.RollBonus(creature, ESavethrow::Reflex), 1);
        EXPECT_EQ(calculator_.RollBonus(creature, ESavethrow::Will), -2);
        EXPECT_EQ(calculator_.RollBonus(creature, ESkill::Athletics), 0);
        EXPECT_EQ(calculator_.RollBonus(creature, TPerceptionTag{}), 12);
        EXPECT_EQ(calculator_.DifficultyClass(creature, TPerceptionTag{}), 22);
    }

    ctx.Undo(checkpoint);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(ProficiencyCalculationTest, MissingEntriesContributeZeroWithoutChangingProfile)
{
    auto ctx = transformator_->ExecutionContext();
    auto& mutable_creature = Creature();
    const auto& creature = mutable_creature;
    ctx.AssignGuarded(mutable_creature.Proficiency(), TProficiency::Create({}),
                      TGuarded<TCreature>(mutable_creature), "proficiency");
    ctx.AssignGuarded(mutable_creature.Level(), 12,
                      TGuarded<TCreature>(mutable_creature), "level");
    const auto before = Snapshot();

    EXPECT_EQ(calculator_.AttackRollBonus(creature, FirstWeapon(creature)), 4);
    EXPECT_EQ(calculator_.ArmorClass(creature, creature), 11);
    EXPECT_EQ(calculator_.RollBonus(creature, ESavethrow::Fortitude), 3);
    EXPECT_EQ(calculator_.RollBonus(creature, ESavethrow::Reflex), 1);
    EXPECT_EQ(calculator_.RollBonus(creature, ESavethrow::Will), 1);
    EXPECT_EQ(calculator_.RollBonus(creature, ESkill::Athletics), 4);
    EXPECT_EQ(calculator_.RollBonus(creature, TPerceptionTag{}), 1);
    EXPECT_EQ(calculator_.DifficultyClass(creature, TPerceptionTag{}), 11);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(ProficiencyCalculationTest, ConditionPenaltiesRemainSeparateFromProficiency)
{
    auto& creature = Creature();
    const auto before = Snapshot();
    const auto checkpoint = transformator_->CurrentState();
    transformator_->ChangeCondition(&creature, EConditionKind::Frightened, 2);
    transformator_->ChangeCondition(&creature, EConditionKind::Prone, 1);

    EXPECT_EQ(creature.WeaponProficiency(EWeaponCategory::Martial), 5);
    EXPECT_EQ(creature.PerceptionProficiency(), 5);
    EXPECT_EQ(calculator_.AttackRollBonus(creature, FirstWeapon(creature)), 5);
    EXPECT_EQ(calculator_.ArmorClass(creature, creature), 10);
    EXPECT_EQ(calculator_.RollBonus(creature, ESavethrow::Fortitude), 6);
    EXPECT_EQ(calculator_.RollBonus(creature, ESkill::Athletics), 5);
    EXPECT_EQ(calculator_.RollBonus(creature, TPerceptionTag{}), 4);
    EXPECT_EQ(calculator_.DifficultyClass(creature, TPerceptionTag{}), 14);

    transformator_->Undo(checkpoint);
    EXPECT_EQ(Snapshot(), before);
}
