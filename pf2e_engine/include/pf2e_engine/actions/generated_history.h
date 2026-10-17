#pragma once

#include <pf2e_engine/actions/action_history.h>
#include <pf2e_engine/actions/generated_source.h>
#include <pf2e_engine/actions/generated_world.h>

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

TRecordedEntityKey CaptureRecordedEntityKey(const std::shared_ptr<TActionContext>& context,
                                            const TGeneratedEntityView& entity);
TRecordedEntityKey CaptureRecordedSourceOwner(const std::shared_ptr<TActionContext>& context,
                                              const TGeneratedSourceKey& source);
TGeneratedEntityView ResolveRecordedEntity(const TGeneratedWorldView& world,
                                            const TRecordedEntityKey& entity);
TGeneratedEntityView ResolveRecordedEntity(const std::shared_ptr<TActionContext>& context,
                                            const TRecordedEntityKey& entity);
const TActionHistory* FindActionHistory(const TGeneratedWorldView& world);
std::optional<TActionRecordStamp> FindActionRecordStamp(const TGeneratedWorldView& world,
                                                       const TPlayer& actor, std::string_view id);
bool MatchActionRecordStamp(const TGeneratedWorldView& world, const TPlayer& actor,
                            const TActionRecordStamp& stamp, std::string_view id);
TActionTicket CaptureActionTicket(const std::shared_ptr<TActionContext>& context);
void AcceptActionExecution(const std::shared_ptr<TActionContext>& context, TPlayer& actor,
                            EActionHistoryKind kind);
bool CurrentActionTicket(const std::shared_ptr<TActionContext>& context, const TActionTicket& ticket);
void StoreActionRecord(const std::shared_ptr<TActionContext>& context, const TActionTicket& ticket,
                       std::shared_ptr<const IActionRecordValue> value);

template <class T>
class TRecordedEntity {
public:
    static TRecordedEntity Capture(const std::shared_ptr<TActionContext>& context,
                                    const TGeneratedEntityBinding<T>& entity)
    {
        TRecordedEntity result;
        result.key_ = CaptureRecordedEntityKey(context, entity.View());
        return result;
    }

    TGeneratedEntityBinding<T> ReadOnly(const TGeneratedWorldView& world) const
    {
        return TGeneratedEntityBinding<T>(ResolveRecordedEntity(world, key_));
    }

    TGeneratedEntityBinding<T> Bind(const std::shared_ptr<TActionContext>& context) const
    {
        return TGeneratedEntityBinding<T>(ResolveRecordedEntity(context, key_));
    }

    TAstNode GetAst(TAstContext& context) const { return key_.GetAst(context); }

private:
    TRecordedEntityKey key_;
};

template <class TOwner, class TSource>
class TRecordedSource {
public:
    static TRecordedSource Capture(const std::shared_ptr<TActionContext>& context,
                                    const TGeneratedSourceBinding<TSource>& source)
    {
        source.Validate(context);
        TRecordedSource result;
        result.key_ = source.Key();
        if (result.key_.OwnerType != typeid(TOwner) || result.key_.SourceType != typeid(TSource)) {
            throw std::invalid_argument("recorded source type mismatch");
        }
        result.owner_ = CaptureRecordedSourceOwner(context, result.key_);
        result.value_ = std::make_shared<const TSource>(source.Read());
        return result;
    }

    TGeneratedSourceBinding<TSource> ReadOnly(const TGeneratedWorldView& world,
                                              const TPlayer& actor) const
    {
        if (!value_ || !owner_.Matches(actor)) {
            throw TAbilityUnavailable();
        }
        const auto owner = TGeneratedEntityBinding<TOwner>(ResolveRecordedEntity(world, owner_));
        auto validate = [owner, key = key_] {
            const auto current = FindAbilitySource<TSource>(owner.Read(), key.Path);
            if (!current || current->Path != key.Path) {
                throw TAbilityUnavailable();
            }
        };
        validate();
        return TGeneratedSourceBinding<TSource>(std::make_shared<const TGeneratedSourceView>(
            typeid(TOwner), typeid(TSource), value_, std::move(validate),
            [owner](const auto& context) {
                ValidateActionEntityMembership(context, owner.View());
            }, key_));
    }

    TGeneratedSourceBinding<TSource> Bind(const std::shared_ptr<TActionContext>& context) const
    {
        const auto actor = GeneratedActionSelf(context);
        return ReadOnly(TGeneratedWorldView(context->battle), actor.Player());
    }

    TAstNode GetAst(TAstContext& context) const
    {
        auto node = TAstNode::MakeObject("TRecordedSource");
        node.AddChild("owner", owner_.GetAst(context));
        AddValueField(node, "source", key_.Path);
        if (value_) {
            AddOwnedObject(node, "value", *value_, context);
        } else {
            node.AddChild("value", TAstNode::MakeNull());
        }
        return node;
    }

private:
    TRecordedEntityKey owner_;
    TGeneratedSourceKey key_;
    std::shared_ptr<const TSource> value_;
};

template <class T>
struct TIsAstRecursive<TRecordedEntity<T>> : std::true_type {};

template <class TOwner, class TSource>
struct TIsAstRecursive<TRecordedSource<TOwner, TSource>> : std::true_type {};

namespace action_record_detail {

template <class TTag>
class TValue final : public IActionRecordValue {
public:
    explicit TValue(typename TTag::TStored value) : value_(std::move(value)) {}
    std::string_view Id() const override { return TTag::kId; }
    std::type_index Type() const override { return typeid(TTag); }
    TAstNode GetAst(TAstContext& context) const override { return TTag::GetAst(value_, context); }
    const typename TTag::TStored& Read() const { return value_; }

private:
    typename TTag::TStored value_;
};

template <class TTag>
class TInput final : public IActionRecordInput {
public:
    explicit TInput(typename TTag::TInputs value) : value_(std::move(value)) {}
    std::type_index Type() const override { return typeid(TTag); }
    const typename TTag::TInputs& Read() const { return value_; }

private:
    typename TTag::TInputs value_;
};

template <class TTag>
const typename TTag::TStored& Stored(const TActionRecordStamp& stamp)
{
    if (!stamp || stamp.Record()->Type() != typeid(TTag) || stamp.Record()->Id() != TTag::kId) {
        throw TAbilityUnavailable();
    }
    const auto* value = dynamic_cast<const TValue<TTag>*>(stamp.Record().get());
    if (value == nullptr) {
        throw std::invalid_argument("action record storage does not match its declared tag");
    }
    return value->Read();
}

}

template <class TTag>
std::optional<typename TTag::TInputs> FindActionRecord(const TGeneratedWorldView& world,
                                                       const TPlayer& actor)
{
    const auto stamp = FindActionRecordStamp(world, actor, TTag::kId);
    if (!stamp) {
        return std::nullopt;
    }
    try {
        return TTag::Rebind(world, actor, action_record_detail::Stored<TTag>(*stamp));
    } catch (const TAbilityUnavailable&) {
        return std::nullopt;
    }
}

template <class TTag>
void PrepareActionRecord(const std::shared_ptr<TActionContext>& context, const TActionRecordStamp& stamp)
{
    const auto actor = GeneratedActionSelf(context);
    if (!MatchActionRecordStamp(TGeneratedWorldView(context->battle), actor.Player(), stamp, TTag::kId)) {
        throw TAbilityUnavailable();
    }
    auto input = TTag::Rebind(context, action_record_detail::Stored<TTag>(stamp));
    context->prepared_record = std::make_shared<const action_record_detail::TInput<TTag>>(std::move(input));
}

template <class TTag>
const typename TTag::TInputs& PreparedActionRecord(const std::shared_ptr<TActionContext>& context)
{
    if (!context || !context->prepared_record || context->prepared_record->Type() != typeid(TTag)) {
        throw TAbilityUnavailable();
    }
    const auto* input = dynamic_cast<const action_record_detail::TInput<TTag>*>(context->prepared_record.get());
    if (input == nullptr) {
        throw std::invalid_argument("prepared action record storage does not match its declared tag");
    }
    return input->Read();
}

template <class TTag>
void PublishActionRecord(const std::shared_ptr<TActionContext>& context, const TActionTicket& ticket,
                         const TGeneratedSourceBinding<typename TTag::TSource>& source,
                         const typename TTag::TArguments& arguments)
{
    if (!CurrentActionTicket(context, ticket)) {
        return;
    }
    GeneratedActionRules(context).Records.Require(TTag::kId, typeid(TTag));
    auto stored = TTag::Detach(context, source, arguments);
    StoreActionRecord(context, ticket,
        std::make_shared<const action_record_detail::TValue<TTag>>(std::move(stored)));
}
