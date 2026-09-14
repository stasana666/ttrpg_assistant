#include <pf2e_engine/actions/generated_source.h>

#include <pf2e_engine/battle.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/player.h>

void ValidateGeneratedSourceActor(const TGeneratedEntityView& owner)
{
    if (!owner.Player().GetCreature()->CanAct()) {
        throw TAbilityUnavailable();
    }
}

const TAbilityRules& GeneratedSourceRules(const std::shared_ptr<TActionContext>& ctx,
                                         const TGeneratedEntityView& owner)
{
    ValidateActionEntity(ctx, owner);
    ValidateGeneratedSourceActor(owner);
    return ctx->battle->AbilityRules();
}

TGeneratedSourceView::TGeneratedSourceView(std::type_index owner_type,
                                           std::type_index source_type,
                                           std::shared_ptr<const void> value,
                                           std::function<void()> validate,
                                           FActionInputValidator validate_context,
                                           std::optional<TGeneratedSourceKey> key)
    : owner_type_(owner_type)
    , source_type_(source_type)
    , value_(std::move(value))
    , validate_(std::move(validate))
    , validate_context_(std::move(validate_context))
    , key_(std::move(key))
{
    if (!value_ || !validate_ || owner_type_ == typeid(void) || source_type_ == typeid(void)) {
        throw std::invalid_argument("prepared ability source requires a typed value and validator");
    }
}

void TGeneratedSourceView::Validate() const
{
    validate_();
}

void TGeneratedSourceView::Validate(const std::shared_ptr<TActionContext>& ctx) const
{
    if (validate_context_) {
        validate_context_(ctx);
    }
    Validate();
}

const TGeneratedSourceKey& TGeneratedSourceView::Key() const
{
    if (!key_) {
        throw std::logic_error("source binding has no stable locator key");
    }
    return *key_;
}

void TGeneratedSourceView::RequireTypes(std::type_index owner_type,
                                       std::type_index source_type) const
{
    if (owner_type_ != owner_type || source_type_ != source_type) {
        throw std::invalid_argument("prepared ability source has different owner or source types");
    }
}
