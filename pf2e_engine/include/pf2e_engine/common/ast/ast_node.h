#pragma once

#include <list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class TAstNode {
public:
    static TAstNode MakeNull();
    static TAstNode MakeValue(std::string serialized);
    static TAstNode MakeObject(std::string_view type_name);

    TAstNode(const TAstNode&) = delete;
    TAstNode& operator =(const TAstNode&) = delete;
    TAstNode(TAstNode&&) = default;
    TAstNode& operator =(TAstNode&&) = default;

    TAstNode* AddChild(std::string_view label, TAstNode child);
    void SetValueContent(std::string serialized);

    bool operator ==(const TAstNode& other) const;
    bool operator !=(const TAstNode& other) const;

    std::string PrettyPrint(int indent = 0) const;
    std::string DiffWith(const TAstNode& other) const;
    std::vector<std::pair<std::string, std::string>> Leaves() const;

private:
    TAstNode() = default;

    std::string DiffImpl(const TAstNode& other, const std::string& path) const;
    static std::string KindName(int kind);
    void CollectLeaves(const std::string& path, std::vector<std::pair<std::string, std::string>>& leaves) const;

    enum class EKind { Null, Value, Object };
    EKind kind_ = EKind::Null;
    std::string content_;
    std::list<std::pair<std::string, TAstNode>> children_;
};
