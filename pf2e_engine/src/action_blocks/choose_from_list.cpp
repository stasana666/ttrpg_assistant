#include <choose_from_list.h>

#include <pf2e_engine/dsl/value.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/game_object_logic/game_object.h>
#include <pf2e_engine/common/visit.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/alternatives.h>
#include <pf2e_engine/i_interaction_system.h>

#include <stdexcept>
#include <string>
#include <variant>

static const TGameObjectId kSelfId = TGameObjectIdManager::Instance().Register("self");
static const TGameObjectId kListId = TGameObjectIdManager::Instance().Register("list");

namespace {

template <class T>
void AddTypedAlternatives(TAlternatives& alternatives, const std::vector<T*>& items);

template <>
void AddTypedAlternatives<TPlayer>(TAlternatives& alternatives, const std::vector<TPlayer*>& items)
{
    for (TPlayer* p : items) {
        alternatives.AddAlternative(std::string(p->GetName()), p);
    }
}

template <>
void AddTypedAlternatives<TWeapon>(TAlternatives& alternatives, const std::vector<TWeapon*>& items)
{
    for (TWeapon* w : items) {
        alternatives.AddAlternative(std::string(w->Name()), w);
    }
}

template <class T>
void PromptAndStore(std::shared_ptr<TActionContext> ctx, TPlayer* self,
                    const std::vector<T*>& items, TGameObjectId output_id,
                    const char* kind)
{
    if (items.empty()) {
        throw std::logic_error(std::string("no targets in list: FChooseFromList (") + kind + ")");
    }
    TAlternatives alternatives = TAlternatives::Create<T*>(kind);
    AddTypedAlternatives<T>(alternatives, items);
    T* chosen = ctx->io_system->ChooseAlternative<T*>(self->GetId(), alternatives);
    ctx->game_object_registry->Add(output_id, chosen);
}

}  // namespace

void FChooseFromList::operator()(std::shared_ptr<TActionContext> ctx) const
{
    TPlayer* self = &ctx->game_object_registry->Get<TPlayer>(kSelfId);

    std::visit(VisitorHelper{
        [&](TPlayerList list) {
            PromptAndStore<TPlayer>(ctx, self, list, output_, "target");
        },
        [&](TWeaponList list) {
            PromptAndStore<TWeapon>(ctx, self, list, output_, "weapon");
        },
        [&](TDslValue::TListPtr list) {
            if (!list || list->empty()) {
                throw std::logic_error("no targets in list: FChooseFromList (dsl)");
            }
            const TDslValue& first = list->front();
            if (first.Is<TPlayer*>()) {
                std::vector<TPlayer*> items;
                items.reserve(list->size());
                for (const auto& v : *list) {
                    items.push_back(v.As<TPlayer*>());
                }
                PromptAndStore<TPlayer>(ctx, self, items, output_, "target");
            } else if (first.Is<TWeapon*>()) {
                std::vector<TWeapon*> items;
                items.reserve(list->size());
                for (const auto& v : *list) {
                    items.push_back(v.As<TWeapon*>());
                }
                PromptAndStore<TWeapon>(ctx, self, items, output_, "weapon");
            } else {
                throw std::logic_error("dsl choose_from_list: unsupported element type");
            }
        },
        [](auto&&) {
            throw std::logic_error("unexpected type for list: FChooseFromList");
        }
    }, input_.Get(kListId, ctx));
}
