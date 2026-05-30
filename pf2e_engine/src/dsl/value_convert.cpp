#include <pf2e_engine/dsl/value_convert.h>

#include <pf2e_engine/common/visit.h>

#include <stdexcept>

TDslValue::TList AsDslList(const TGameObjectPtr& obj)
{
    return std::visit(VisitorHelper{
        [](TPlayerList list) -> TDslValue::TList {
            TDslValue::TList items;
            items.reserve(list.size());
            for (TPlayer* p : list) {
                items.emplace_back(p);
            }
            return items;
        },
        [](TWeaponList list) -> TDslValue::TList {
            TDslValue::TList items;
            items.reserve(list.size());
            for (TWeapon* w : list) {
                items.emplace_back(w);
            }
            return items;
        },
        [](TDslValue::TListPtr list) -> TDslValue::TList {
            if (!list) {
                return {};
            }
            return *list;
        },
        [](auto&&) -> TDslValue::TList {
            throw std::runtime_error("dsl: value is not a list");
        }
    }, obj);
}

TGameObjectPtr ToGameObjectPtr(const TDslValue& v)
{
    return std::visit(VisitorHelper{
        [](std::monostate) -> TGameObjectPtr {
            throw std::runtime_error("dsl: cannot store void in registry");
        },
        [](bool) -> TGameObjectPtr {
            throw std::runtime_error("dsl: cannot store bool in registry");
        },
        [](int i) -> TGameObjectPtr { return TGameObjectPtr(i); },
        [](TArmor* p) -> TGameObjectPtr { return TGameObjectPtr(p); },
        [](TWeapon* p) -> TGameObjectPtr { return TGameObjectPtr(p); },
        [](TCreature* p) -> TGameObjectPtr { return TGameObjectPtr(p); },
        [](TPlayer* p) -> TGameObjectPtr { return TGameObjectPtr(p); },
        [](const TDslValue::TListPtr& list) -> TGameObjectPtr {
            if (!list) {
                return std::make_shared<const TDslValue::TList>();
            }
            return list;
        }
    }, v.data);
}
