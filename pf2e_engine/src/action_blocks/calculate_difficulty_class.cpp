#include <calculate_difficulty_class.h>

#include <pf2e_engine/player.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/common/errors.h>

static const TGameObjectId kTypeId = TGameObjectIdManager::Instance().Register("type");
static const TGameObjectId kTargetId = TGameObjectIdManager::Instance().Register("creature");

enum class EDifficultyClassType {
    Skill,
    ArmorClass,
};

EDifficultyClassType DifficultyClassTypeFromString(const std::string& s) {
    if (s == "skill") {
        return EDifficultyClassType::Skill;
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

void FCalculateDifficultyClass::SkillDifficultyClassHandle(TActionContext&) const
{
    throw ToDoError("FCalculateDifficultyClass::SkillDifficultyClassHandle(TActionContext& ctx)");
}
