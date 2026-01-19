#include <choose_from_list.h>

#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/game_object_logic/game_object.h>
#include <pf2e_engine/common/visit.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/alternatives.h>
#include <pf2e_engine/interaction_system.h>

#include <stdexcept>
#include <variant>

static const TGameObjectId kSelfId = TGameObjectIdManager::Instance().Register("self");
static const TGameObjectId kListId = TGameObjectIdManager::Instance().Register("list");

void FChooseFromList::operator()(std::shared_ptr<TActionContext> ctx) const
{
    TPlayer* self = &ctx->game_object_registry->Get<TPlayer>(kSelfId);

    TPlayerList targets;
    std::visit(VisitorHelper{
        [&](TPlayerList list) {
            targets = std::move(list);
        },
        [](auto&&) {
            throw std::logic_error("unexpected type for list: FChooseFromList");
        }
    }, input_.Get(kListId, ctx));

    if (targets.empty()) {
        throw std::logic_error("no targets in list: FChooseFromList");
    }

    TAlternatives alternatives = TAlternatives::Create<TPlayer*>("target");
    for (TPlayer* p : targets) {
        alternatives.AddAlternative(std::string(p->GetName()), p);
    }

    TPlayer* chosen = ctx->io_system->ChooseAlternative<TPlayer*>(self->GetId(), alternatives);
    ctx->game_object_registry->Add(output_, chosen);
}
