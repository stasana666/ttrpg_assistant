#pragma once

#include <concepts>
#include <memory>
#include <variant>
#include <vector>

class TArmor;
class TWeapon;
class TCreature;
class TPlayer;

class TDslValue {
public:
    using TList = std::vector<TDslValue>;
    using TListPtr = std::shared_ptr<const TList>;

    using V = std::variant<
        std::monostate,
        bool,
        int,
        TArmor*,
        TWeapon*,
        TCreature*,
        TPlayer*,
        TListPtr
    >;

    V data;

    TDslValue() = default;
    explicit TDslValue(V v) : data(std::move(v)) {}
    template <class B>
        requires std::same_as<B, bool>
    explicit TDslValue(B b) : data(b) {}
    explicit TDslValue(int i) : data(i) {}
    explicit TDslValue(TArmor* p) : data(p) {}
    explicit TDslValue(TWeapon* p) : data(p) {}
    explicit TDslValue(TCreature* p) : data(p) {}
    explicit TDslValue(TPlayer* p) : data(p) {}
    explicit TDslValue(TListPtr l) : data(std::move(l)) {}

    static TDslValue MakeList(TList items) {
        return TDslValue(std::make_shared<const TList>(std::move(items)));
    }

    template <class T>
    bool Is() const { return std::holds_alternative<T>(data); }

    template <class T>
    const T& As() const { return std::get<T>(data); }

    bool AsBool() const { return std::get<bool>(data); }
    int AsInt() const { return std::get<int>(data); }
    const TList& AsList() const { return *std::get<TListPtr>(data); }
};
