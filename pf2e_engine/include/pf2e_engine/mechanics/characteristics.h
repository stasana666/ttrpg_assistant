#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/observable.h>

#include <array>
#include <string_view>
#include <string>

enum class ECharacteristic {
    Strength,
    Dexterity,
    Constitution,
    Intelligence,
    Wisdom,
    Charisma,
};

class TCharacteristic final : public TObservable<const TCharacteristic&> {
public:
    explicit TCharacteristic(int value);

    int GetMod() const;
    int GetValue() const;
    void Set(int value);

    // Only `value_` is emitted; the inherited TObservable subscribers_ list
    // is runtime callbacks, not save/rollback state.
    TAstNode GetAst(TAstContext& ctx) const;

private:
    int value_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TCharacteristic> : std::true_type {};

class TCharacteristicSet {
public:
    static constexpr size_t kCharacteristicCount = 6;

public:
    explicit TCharacteristicSet(std::array<int, kCharacteristicCount> values);

    TCharacteristic& operator[](ECharacteristic stat);
    const TCharacteristic& operator[](ECharacteristic stat) const;

    TAstNode GetAst(TAstContext& ctx) const;

private:
    std::array<TCharacteristic, kCharacteristicCount> stats_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TCharacteristicSet> : std::true_type {};

ECharacteristic CharacteristicFromString(std::string_view);
std::string ToString(ECharacteristic);
