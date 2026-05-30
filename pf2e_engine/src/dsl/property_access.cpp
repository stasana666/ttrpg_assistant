#include <pf2e_engine/dsl/property_access.h>

#include <pf2e_engine/dsl/property_registry.h>
#include <pf2e_engine/common/visit.h>

#include <stdexcept>

TDslValue GetDslProperty(const TDslValue& receiver, const std::string& name, TEvalContext& ctx)
{
    return std::visit(VisitorHelper{
        [&](TArmor* p) -> TDslValue {
            return TPropertyRegistry<TArmor>::Instance().Get(p, name, ctx);
        },
        [&](TWeapon* p) -> TDslValue {
            return TPropertyRegistry<TWeapon>::Instance().Get(p, name, ctx);
        },
        [&](TCreature* p) -> TDslValue {
            return TPropertyRegistry<TCreature>::Instance().Get(p, name, ctx);
        },
        [&](TPlayer* p) -> TDslValue {
            return TPropertyRegistry<TPlayer>::Instance().Get(p, name, ctx);
        },
        [&](TDslValue::TListPtr) -> TDslValue {
            throw std::runtime_error("dsl: property '" + name + "' on list");
        },
        [&](auto&&) -> TDslValue {
            throw std::runtime_error("dsl: property '" + name + "' on non-object receiver");
        }
    }, receiver.data);
}
