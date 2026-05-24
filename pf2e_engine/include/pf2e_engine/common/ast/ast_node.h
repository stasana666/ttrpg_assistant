#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

class TAstNode {
public:
    static TAstNode MakeNull();
    static TAstNode MakeValue(std::string serialized);
    static TAstNode MakeObject(std::string_view type_name);

    TAstNode& AddChild(std::string_view label, TAstNode child);

    bool operator ==(const TAstNode& other) const;
    bool operator !=(const TAstNode& other) const;

    std::string PrettyPrint(int indent = 0) const;

    // Returns "" if equal, otherwise a single line describing the first
    // diverging /path and the kind of mismatch.
    std::string DiffWith(const TAstNode& other) const;

private:
    TAstNode() = default;

    std::string DiffImpl(const TAstNode& other, const std::string& path) const;
    static std::string KindName(int kind);

    enum class EKind { Null, Value, Object };
    EKind kind_ = EKind::Null;
    std::string content_;  // serialized value for Value; type_name for Object
    std::vector<std::pair<std::string, TAstNode>> children_;
};
