#include <pf2e_engine/actions/action_history.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/transformation/transformator.h>

#include <limits>
#include <utility>

bool TRecordedEntityKey::Matches(const TPlayer& player) const
{
    const auto* creature = player.GetCreature();
    return Identity == &player && PlayerId == player.GetId() && creature != nullptr
        && Address == creature && Type == typeid(TCreature) && Id == creature->Id();
}

TAstNode TRecordedEntityKey::GetAst(TAstContext& context) const
{
    AST_ASSERT_LAYOUT(TRecordedEntityKey, 40);
    auto node = TAstNode::MakeObject("TRecordedEntityKey");
    AddValueField(node, "player_id", PlayerId);
    AddValueField(node, "entity_type", Type == typeid(TCreature) ? "TCreature" : "invalid");
    AddValueField(node, "identity_valid", Identity != nullptr && Matches(*Identity));
    AddReference(node, "player", Identity, context);
    AddReference(node, "entity", Address, context);
    return node;
}

void TActionRecordRegistry::RegisterType(std::string id, std::type_index type)
{
    if (id.empty() || type == typeid(void)) {
        throw std::invalid_argument("action record requires an identity and type");
    }
    const auto [found, inserted] = types_.emplace(std::move(id), type);
    if (!inserted && found->second != type) {
        throw std::invalid_argument("conflicting action record type");
    }
}

void TActionRecordRegistry::Require(std::string_view id, std::type_index type) const
{
    const auto found = types_.find(id);
    if (found == types_.end() || found->second != type) {
        throw std::invalid_argument("missing or mismatched action record type");
    }
}

std::optional<TActionRecordStamp> TActionHistory::Find(const TPlayer& actor, std::string_view record) const
{
    const auto found = state_.Actors.find(actor.GetId());
    if (found == state_.Actors.end() || !found->second.Version->Actor.Matches(actor)
        || !found->second.Record || found->second.Record->Id() != record) {
        return std::nullopt;
    }
    TActionRecordStamp stamp;
    stamp.version_ = found->second.Version;
    stamp.record_ = found->second.Record;
    return stamp;
}

bool TActionHistory::Matches(const TPlayer& actor, const TActionRecordStamp& stamp,
                             std::string_view record) const
{
    const auto current = Find(actor, record);
    return current && current->version_ == stamp.version_ && current->record_ == stamp.record_;
}

bool TActionHistory::Current(const TPlayer& actor, const TActionTicket& ticket) const
{
    const auto found = state_.Actors.find(actor.GetId());
    return ticket.version_ && ticket.version_->History == this
        && ticket.version_->Actor.Matches(actor) && found != state_.Actors.end()
        && found->second.Version == ticket.version_ && !found->second.Record;
}

std::uint64_t TActionHistory::Revision(const TPlayer& actor) const
{
    const auto found = state_.Actors.find(actor.GetId());
    return found == state_.Actors.end() || !found->second.Version->Actor.Matches(actor)
        ? 0 : found->second.Version->Revision;
}

TActionTicket TActionHistory::Accept(TPlayer& actor, EActionHistoryKind kind,
                                    std::string definition, TTransformator& journal)
{
    const auto* creature = std::as_const(actor).GetCreature();
    if (creature == nullptr || creature->Id().Value == 0) {
        throw std::invalid_argument("action history requires an entity participant");
    }
    if (const auto existing = state_.Actors.find(actor.GetId()); existing != state_.Actors.end()
        && !existing->second.Version->Actor.Matches(actor)) {
        throw std::invalid_argument("action history actor identity does not match the enrolled participant");
    }
    const auto revision = Revision(actor);
    if (revision == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("action history revision overflow");
    }
    TActionTicket ticket;
    ticket.version_ = std::make_shared<const TActionHistoryVersion>(TActionHistoryVersion{
        this, {creature->Id(), actor.GetId(), &actor, creature, typeid(TCreature)},
        revision + 1, kind, std::move(definition),
    });
    auto next = state_;
    next.Actors.insert_or_assign(actor.GetId(), TEntry{ticket.version_, {}});
    auto context = journal.ExecutionContext();
    context.Assign(state_, std::move(next), this, "state");
    return ticket;
}

bool TActionHistory::Publish(const TPlayer& actor, const TActionTicket& ticket,
                             std::shared_ptr<const IActionRecordValue> record, TTransformator& journal)
{
    if (!Current(actor, ticket)) {
        return false;
    }
    if (!record || record->Id().empty() || ticket.version_->Kind != EActionHistoryKind::Action) {
        throw std::invalid_argument("only an accepted root action can publish an action record");
    }
    auto next = state_;
    next.Actors.at(actor.GetId()).Record = std::move(record);
    auto context = journal.ExecutionContext();
    context.Assign(state_, std::move(next), this, "state");
    return true;
}

TAstNode TActionHistory::TStateData::GetAst(TAstContext& context) const
{
    AST_ASSERT_LAYOUT(TStateData, 48);
    auto node = TAstNode::MakeObject("TActionHistoryState");
    for (const auto& [id, entry] : Actors) {
        auto actor = TAstNode::MakeObject("entry");
        actor.AddChild("actor", entry.Version->Actor.GetAst(context));
        AddValueField(actor, "revision", entry.Version->Revision);
        AddValueField(actor, "kind", entry.Version->Kind);
        AddValueField(actor, "definition", entry.Version->Definition);
        if (entry.Record) {
            AddValueField(actor, "record_id", entry.Record->Id());
            actor.AddChild("record", entry.Record->GetAst(context));
        } else {
            actor.AddChild("record", TAstNode::MakeNull());
        }
        node.AddChild(std::to_string(id), std::move(actor));
    }
    return node;
}

TAstNode TActionHistory::GetAst(TAstContext& context) const
{
    AST_ASSERT_LAYOUT(TActionHistory, 56);
    return state_.GetAst(context);
}
