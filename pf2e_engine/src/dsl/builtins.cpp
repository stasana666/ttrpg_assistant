#include <pf2e_engine/dsl/builtins.h>

#include <pf2e_engine/dsl/function_registry.h>
#include <pf2e_engine/dsl/property_registry.h>
#include <pf2e_engine/dsl/value.h>

#include <pf2e_engine/battle.h>
#include <pf2e_engine/battle_map.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/player.h>

#include <algorithm>
#include <stdexcept>

namespace {

TPlayer* RequirePlayer(const TDslValue& v, const char* fn) {
    if (!v.Is<TPlayer*>()) {
        throw std::runtime_error(std::string("dsl: function '") + fn + "' requires TPlayer*");
    }
    return v.As<TPlayer*>();
}

void RegisterAll() {
    // Properties on TWeapon
    auto& weapon_props = TPropertyRegistry<TWeapon>::Instance();
    weapon_props.Register("reach", [](const TWeapon* w, TEvalContext&) {
        return TDslValue(w->Reach());
    });

    // Properties on TPlayer
    auto& player_props = TPropertyRegistry<TPlayer>::Instance();
    player_props.Register("creature", [](TPlayer* p, TEvalContext&) {
        return TDslValue(p->GetCreature());
    });
    player_props.Register("weapons", [](TPlayer* p, TEvalContext&) {
        TCreature* c = p->GetCreature();
        TDslValue::TList items;
        auto& slots = c->Weapons();
        for (size_t i = 0; i < slots.Size(); ++i) {
            items.emplace_back(&slots[i].Weapon());
        }
        for (TWeapon& w : c->NaturalWeapons()) {
            items.emplace_back(&w);
        }
        return TDslValue::MakeList(std::move(items));
    });

    // Functions
    auto& fns = TDslFunctionRegistry::Instance();

    fns.Register("creatures", [](const std::vector<TDslValue>& args, TEvalContext& ctx) {
        if (!args.empty()) {
            throw std::runtime_error("dsl: creatures() takes no arguments");
        }
        if (ctx.battle == nullptr) {
            throw std::runtime_error("dsl: creatures() requires a battle in eval context");
        }
        TPlayerList living = ctx.battle->GetIfPlayers([](const TPlayer* p) {
            return p->GetCreature()->IsAlive();
        });
        TDslValue::TList items;
        items.reserve(living.size());
        for (TPlayer* p : living) {
            items.emplace_back(p);
        }
        return TDslValue::MakeList(std::move(items));
    });

    fns.Register("distance", [](const std::vector<TDslValue>& args, TEvalContext& ctx) {
        if (args.size() != 2) {
            throw std::runtime_error("dsl: distance() takes 2 arguments");
        }
        TPlayer* a = RequirePlayer(args[0], "distance");
        TPlayer* b = RequirePlayer(args[1], "distance");
        TPosition pa = a->GetPosition();
        TPosition pb = b->GetPosition();
        // Chebyshev distance — matches TBattleMap::HasLine/InRadius semantics.
        // (BattleMap doesn't expose a Distance() helper; replicate here.)
        int dx = pa.x - pb.x;
        if (dx < 0) {
            dx = -dx;
        }
        int dy = pa.y - pb.y;
        if (dy < 0) {
            dy = -dy;
        }
        (void)ctx;
        return TDslValue(std::max(dx, dy));
    });

    fns.Register("has_line_of_effect", [](const std::vector<TDslValue>& args, TEvalContext& ctx) {
        if (args.size() != 2) {
            throw std::runtime_error("dsl: has_line_of_effect() takes 2 arguments");
        }
        if (ctx.battle == nullptr) {
            throw std::runtime_error("dsl: has_line_of_effect() requires a battle");
        }
        TPlayer* a = RequirePlayer(args[0], "has_line_of_effect");
        TPlayer* b = RequirePlayer(args[1], "has_line_of_effect");
        return TDslValue(ctx.battle->BattleMap()->HasLine(a->GetPosition(), b->GetPosition()));
    });

    fns.Register("min", [](const std::vector<TDslValue>& args, TEvalContext&) {
        if (args.size() != 2) {
            throw std::runtime_error("dsl: min() takes 2 arguments");
        }
        if (!args[0].Is<int>() || !args[1].Is<int>()) {
            throw std::runtime_error("dsl: min() requires int arguments");
        }
        return TDslValue(std::min(args[0].AsInt(), args[1].AsInt()));
    });

    fns.Register("max", [](const std::vector<TDslValue>& args, TEvalContext&) {
        if (args.size() != 2) {
            throw std::runtime_error("dsl: max() takes 2 arguments");
        }
        if (!args[0].Is<int>() || !args[1].Is<int>()) {
            throw std::runtime_error("dsl: max() requires int arguments");
        }
        return TDslValue(std::max(args[0].AsInt(), args[1].AsInt()));
    });
}

}  // namespace

void EnsureDslBuiltinsRegistered()
{
    static bool registered = []() {
        RegisterAll();
        return true;
    }();
    (void)registered;
}
