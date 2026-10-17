#pragma once

#include <pf2e_engine/creature.h>

#include <map>
#include <utility>

inline TCreature MakeWarrior()
{
    const auto rank = [](EProficiencyRank value) { return TProficiencyValue::Create({.Rank = value}); };
    const auto trained = rank(EProficiencyRank::Trained);
    const auto expert = rank(EProficiencyRank::Expert);
    TIdCollection<TWeapon> weapons;
    weapons.Add(TWeapon::Create({.Name = "longsword", .BaseDiceSize = 6, .DamageType = EDamageType::Slashing,
                                 .Category = EWeaponCategory::Martial}));
    return TCreature::Create({
        .Level = 1,
        .Size = ECreatureSize::Medium,
        .Race = TRace::Create({.Hitpoints = 8}),
        .Class = TClass::Create({.Hitpoints = 10}),
        .Characteristic = TAbilityScores::Create({
            .Strength = TAbilityScore::Create({.Value = 18}), .Dexterity = TAbilityScore::Create({.Value = 12}),
            .Constitution = TAbilityScore::Create({.Value = 16}), .Intelligence = TAbilityScore::Create({.Value = 12}),
            .Wisdom = TAbilityScore::Create({.Value = 13}), .Charisma = TAbilityScore::Create({.Value = 13}),
        }),
        .Proficiency = TProficiency::Create({
            .ArmorCategory = std::map<EArmorCategory, TProficiencyValue>{
                {EArmorCategory::Unarmored, trained}, {EArmorCategory::Light, trained},
                {EArmorCategory::Medium, trained}, {EArmorCategory::Heavy, trained}},
            .WeaponCategory = std::map<EWeaponCategory, TProficiencyValue>{
                {EWeaponCategory::Unarmed, expert}, {EWeaponCategory::Simple, expert},
                {EWeaponCategory::Martial, expert}},
            .Savethrow = std::map<ESavethrow, TProficiencyValue>{
                {ESavethrow::Fortitude, expert}, {ESavethrow::Reflex, expert}, {ESavethrow::Will, trained}},
            .Skill = std::map<ESkill, TProficiencyValue>{
                {ESkill::Acrobatics, trained}, {ESkill::Athletics, trained}, {ESkill::Deception, trained},
                {ESkill::Diplomacy, trained}, {ESkill::Intimidation, trained}},
            .Perception = expert,
        }),
        .Movement = 5,
        .Weapons = std::move(weapons),
    });
}
