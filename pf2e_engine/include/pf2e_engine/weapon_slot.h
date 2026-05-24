#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/observable.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/resources.h>

class TWeaponSlots {
public:
    class TWeaponDescriptor {
    public:
        void SetGrip(size_t hand_count);

        int Grip() const;
        TWeapon& Weapon();
        const TWeapon& Weapon() const;

    private:
        friend TWeaponSlots;

        TWeaponDescriptor(TWeaponSlots* parent, size_t index);

        TWeaponSlots* parent_;
        size_t index_;
    };

    struct THoldedWeapon {
        TWeapon weapon;
        int hand_count;

        TAstNode GetAst(TAstContext& ctx) const;
    };

    TWeaponDescriptor Equip(THoldedWeapon weapon);

    TWeaponDescriptor operator [](size_t idx);
    const TWeapon& WeaponAt(size_t idx) const;

    size_t Size() const;
    bool Empty() const;

    TAstNode GetAst(TAstContext& ctx) const;

private:
    std::vector<THoldedWeapon> weapons_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TWeaponSlots> : std::true_type {};

template <>
struct TIsAstRecursive<TWeaponSlots::THoldedWeapon> : std::true_type {};
