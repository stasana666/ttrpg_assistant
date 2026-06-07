#include <spell_damage_roll.h>

#include <pf2e_engine/alternatives.h>
#include <pf2e_engine/i_interaction_system.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/mechanics/damage.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/transformation/transformator.h>

#include <stdexcept>

static const TGameObjectId kCasterId = TGameObjectIdManager::Instance().Register("caster");
static const TGameObjectId kDamageTableId = TGameObjectIdManager::Instance().Register("damage_table");
static const TGameObjectId kDamageTypeId = TGameObjectIdManager::Instance().Register("damage_type");

class TSharedExpressionWrapper : public IExpression {
public:
    explicit TSharedExpressionWrapper(std::shared_ptr<IExpression> expr)
        : expr_(std::move(expr)) {}

    int Value(IRandomGenerator& rng) const override {
        return expr_->Value(rng);
    }

private:
    std::shared_ptr<IExpression> expr_;
};

void FSpellDamageRoll::operator()(std::shared_ptr<TActionContext> ctx) const
{
    TPlayer* caster = std::get<TPlayer*>(input_.Get(kCasterId, ctx));
    const TDamageTable& damage_table = input_.GetDamageTable(kDamageTableId);
    std::string damage_type_str = input_.GetString(kDamageTypeId);
    EDamageType damage_type = EDamageTypeFromString(damage_type_str);

    TAlternatives alternatives = TAlternatives::Create<std::string>("spell_slot");

    const auto& slots = caster->GetCreature()->SpellSlots();
    for (const auto& [slot_name, expression] : damage_table) {
        ESpellSlotRank rank = ESpellSlotRankFromString(slot_name);
        auto it = slots->find(rank);
        if (it != slots->end() && it->second.Has(1)) {
            alternatives.AddAlternative(slot_name, slot_name);
        }
    }

    if (alternatives.Empty()) {
        throw std::runtime_error("No spell slots available for this spell");
    }

    std::string chosen_slot = ctx->io_system->ChooseAlternative<std::string>(
        caster->GetId(), alternatives);
    ESpellSlotRank chosen_rank = ESpellSlotRankFromString(chosen_slot);
    ctx->transformator->ReduceSpellSlot(caster->GetCreature()->SpellSlots(), chosen_rank, 1);

    auto damage = std::make_shared<TDamage>();
    const auto& expr = damage_table.at(chosen_slot);
    damage->Add(damage_type, std::make_unique<TSharedExpressionWrapper>(expr));

    ctx->game_object_registry->Add(output_, damage);
}
