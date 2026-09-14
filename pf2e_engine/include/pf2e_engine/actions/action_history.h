#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/entity_id.h>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>

class TPlayer;
class TTransformator;
class TActionHistory;

struct TRecordedEntityKey {
    TEntityId Id;
    int PlayerId = 0;
    const TPlayer* Identity = nullptr;
    const void* Address = nullptr;
    std::type_index Type = typeid(void);

    bool Matches(const TPlayer& player) const;
    TAstNode GetAst(TAstContext& context) const;
};

enum class EActionHistoryKind {
    Action,
    Reaction,
};

class IActionRecordValue {
public:
    virtual ~IActionRecordValue() = default;
    virtual std::string_view Id() const = 0;
    virtual std::type_index Type() const = 0;
    virtual TAstNode GetAst(TAstContext& context) const = 0;
};

class IActionRecordInput {
public:
    virtual ~IActionRecordInput() = default;
    virtual std::type_index Type() const = 0;
};

struct TActionHistoryVersion {
    const TActionHistory* History;
    TRecordedEntityKey Actor;
    std::uint64_t Revision;
    EActionHistoryKind Kind;
    std::string Definition;
};

class TActionTicket {
public:
    TActionTicket() = default;
    explicit operator bool() const { return version_ != nullptr; }

private:
    friend class TActionHistory;
    std::shared_ptr<const TActionHistoryVersion> version_;
};

class TActionRecordStamp {
public:
    TActionRecordStamp() = default;
    explicit operator bool() const { return version_ != nullptr && record_ != nullptr; }
    const std::shared_ptr<const IActionRecordValue>& Record() const { return record_; }

private:
    friend class TActionHistory;
    std::shared_ptr<const TActionHistoryVersion> version_;
    std::shared_ptr<const IActionRecordValue> record_;
};

class TActionRecordRegistry {
public:
    template <class TTag>
    void Register()
    {
        RegisterType(std::string(TTag::kId), typeid(TTag));
    }

    void Require(std::string_view id, std::type_index type) const;

private:
    void RegisterType(std::string id, std::type_index type);
    std::map<std::string, std::type_index, std::less<>> types_;
};

class TActionHistory {
public:
    TActionHistory() = default;
    TActionHistory(const TActionHistory&) = delete;
    TActionHistory& operator=(const TActionHistory&) = delete;
    TActionHistory(TActionHistory&&) = delete;
    TActionHistory& operator=(TActionHistory&&) = delete;

    std::optional<TActionRecordStamp> Find(const TPlayer& actor, std::string_view record) const;
    bool Matches(const TPlayer& actor, const TActionRecordStamp& stamp, std::string_view record) const;
    bool Current(const TPlayer& actor, const TActionTicket& ticket) const;
    std::uint64_t Revision(const TPlayer& actor) const;
    TActionTicket Accept(TPlayer& actor, EActionHistoryKind kind, std::string definition,
                          TTransformator& journal);
    bool Publish(const TPlayer& actor, const TActionTicket& ticket,
                 std::shared_ptr<const IActionRecordValue> record, TTransformator& journal);
    TAstNode GetAst(TAstContext& context) const;

private:
    struct TEntry {
        std::shared_ptr<const TActionHistoryVersion> Version;
        std::shared_ptr<const IActionRecordValue> Record;
    };

public:
    struct TStateData {
        std::map<int, TEntry> Actors;
        TAstNode GetAst(TAstContext& context) const;
    };

private:
    TStateData state_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TRecordedEntityKey> : std::true_type {};

template <>
struct TIsAstRecursive<TActionHistory> : std::true_type {};

template <>
struct TIsAstRecursive<TActionHistory::TStateData> : std::true_type {};
