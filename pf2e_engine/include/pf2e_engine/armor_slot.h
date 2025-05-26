#pragma once

#include <pf2e_engine/common/observable.h>
#include <pf2e_engine/inventory/armor.h>

class TArmorSlot final : public TObservable<const TArmor&> {
public:
    TArmorSlot();
    explicit TArmorSlot(const TArmor* armor);

    void Set(const TArmor* armor);
    const TArmor& Get() const;

private:
    const TArmor* armor;
};
