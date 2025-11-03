#include <calculate_difficulty_class.h>

#include <pf2e_engine/player.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/common/errors.h>
#include "savethrows.h"
#include "skills.h"

static const TGameObjectId kTypeId = TGameObjectIdManager::Instance().Register("type");
static const TGameObjectId kTargetId = TGameObjectIdManager::Instance().Register("creature");
static const TGameObjectId kSkillId = TGameObjectIdManager::Instance().Register("skill");
static const TGameObjectId kSavethrowId = TGameObjectIdManager::Instance().Register("savethrow");

enum class EDifficultyClassType {
    ArmorClass,
    Savethrow,
    Skill,
};

EDifficultyClassType DifficultyClassTypeFromString(const std::string& s) {
    if (s == "skill") {
        return EDifficultyClassType::Skill;
    }
    if (s == "savethrow") {
        return EDifficultyClassType::Savethrow;
    }
    if (s == "armor_class") {
        return EDifficultyClassType::ArmorClass;
    }
    throw std::invalid_argument("unknown DC type: " + s);
}

void FCalculateDifficultyClass::operator() (TActionContext& ctx) const
{
    switch (DifficultyClassTypeFromString(input_.GetString(kTypeId))) {
        case EDifficultyClassType::ArmorClass:
            return ArmorClassHandle(ctx);
        case EDifficultyClassType::Savethrow:
            return SavethrowDifficultyClassHandle(ctx);
        case EDifficultyClassType::Skill:
            return SkillDifficultyClassHandle(ctx);
    }
    throw std::invalid_argument("invalid DC type: FCalculateDifficultyClass::operator()");
}

void FCalculateDifficultyClass::ArmorClassHandle(TActionContext& ctx) const
{
    TPlayer& target = *std::get<TPlayer*>(input_.Get(kTargetId, ctx));
    int armor_class = calculator_.ArmorClass(*target.creature);
    ctx.game_object_registry->Add(output_, armor_class);
}

void FCalculateDifficultyClass::SkillDifficultyClassHandle(TActionContext& ctx) const
{
    TPlayer& target = *std::get<TPlayer*>(input_.Get(kTargetId, ctx));
    ESkill skill = SkillFromString(input_.GetString(kSkillId));

    int difficulty_class = calculator_.DifficultyClass(*target.creature, skill);
    ctx.game_object_registry->Add(output_, difficulty_class);
}

void FCalculateDifficultyClass::SavethrowDifficultyClassHandle(TActionContext& ctx) const
{
    TPlayer& target = *std::get<TPlayer*>(input_.Get(kTargetId, ctx));
    ESavethrow savethrow = SavethrowFromString(input_.GetString(kSavethrowId));

    int difficulty_class = calculator_.DifficultyClass(*target.creature, savethrow);
    ctx.game_object_registry->Add(output_, difficulty_class);
}
