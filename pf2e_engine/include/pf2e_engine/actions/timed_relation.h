#pragma once

#include <pf2e_engine/actions/action_history.h>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <typeindex>

struct TRelationAddress {
    std::string Definition;
    TRecordedEntityKey Owner;
    TRecordedEntityKey Key;
};

class IRelationValue {
public:
    virtual ~IRelationValue() = default;
    virtual std::string_view Id() const = 0;
    virtual std::type_index Type() const = 0;
    virtual TAstNode GetAst(TAstContext& context) const = 0;
};

class TRelationRegistry {
public:
    template <class TTag>
    void Register()
    {
        RegisterType(std::string(TTag::kId), typeid(TTag), typeid(typename TTag::TOwner),
                     typeid(typename TTag::TKey));
    }

    void Require(std::string_view id, std::type_index type,
                 std::type_index owner, std::type_index key) const;

private:
    struct TType {
        std::type_index Tag;
        std::type_index Owner;
        std::type_index Key;
        friend bool operator==(const TType&, const TType&) = default;
    };
    void RegisterType(std::string id, std::type_index type, std::type_index owner, std::type_index key);
    std::map<std::string, TType, std::less<>> types_;
};

class TTimedRelationStore {
public:
    const IRelationValue* Find(const TRelationAddress& address, std::uint64_t now) const;
    void Set(TRelationAddress address, std::optional<std::uint64_t> expires_at,
             std::shared_ptr<const IRelationValue> value, TTransformator& journal);
    void Erase(const TRelationAddress& address, TTransformator& journal);
    TAstNode GetAst(TAstContext& context) const;

private:
    using TIndex = std::tuple<std::string, int, int>;

    struct TEntry {
        TRelationAddress Address;
        std::optional<std::uint64_t> ExpiresAt;
        std::shared_ptr<const IRelationValue> Value;
    };

public:
    struct TStateData {
        std::map<TIndex, TEntry> Entries;
        TAstNode GetAst(TAstContext& context) const;
    };

private:
    static TIndex Index(const TRelationAddress& address);
    TStateData state_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TTimedRelationStore> : std::true_type {};

template <>
struct TIsAstRecursive<TTimedRelationStore::TStateData> : std::true_type {};
