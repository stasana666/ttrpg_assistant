#include <pf2e_engine/common/ast/ast_node.h>

#include <sstream>

TAstNode TAstNode::MakeNull()
{
    TAstNode n;
    n.kind_ = EKind::Null;
    return n;
}

TAstNode TAstNode::MakeValue(std::string serialized)
{
    TAstNode n;
    n.kind_ = EKind::Value;
    n.content_ = std::move(serialized);
    return n;
}

TAstNode TAstNode::MakeObject(std::string_view type_name)
{
    TAstNode n;
    n.kind_ = EKind::Object;
    n.content_ = std::string(type_name);
    return n;
}

TAstNode* TAstNode::AddChild(std::string_view label, TAstNode child)
{
    children_.emplace_back(std::string(label), std::move(child));
    return &children_.back().second;
}

void TAstNode::SetValueContent(std::string serialized)
{
    kind_ = EKind::Value;
    content_ = std::move(serialized);
}

bool TAstNode::operator ==(const TAstNode& other) const
{
    if (kind_ != other.kind_) {
        return false;
    }
    if (content_ != other.content_) {
        return false;
    }
    if (children_.size() != other.children_.size()) {
        return false;
    }
    auto it_a = children_.begin();
    auto it_b = other.children_.begin();
    for (; it_a != children_.end(); ++it_a, ++it_b) {
        if (it_a->first != it_b->first) {
            return false;
        }
        if (it_a->second != it_b->second) {
            return false;
        }
    }
    return true;
}

bool TAstNode::operator !=(const TAstNode& other) const
{
    return !(*this == other);
}

static void IndentTo(std::ostringstream& oss, int n)
{
    for (int i = 0; i < n; ++i) {
        oss << "  ";
    }
}

std::string TAstNode::PrettyPrint(int indent) const
{
    std::ostringstream oss;
    switch (kind_) {
    case EKind::Null:
        IndentTo(oss, indent);
        oss << "<null>";
        break;
    case EKind::Value:
        IndentTo(oss, indent);
        oss << content_;
        break;
    case EKind::Object:
        IndentTo(oss, indent);
        oss << content_ << " {";
        if (children_.empty()) {
            oss << "}";
        } else {
            oss << "\n";
            for (const auto& [label, child] : children_) {
                IndentTo(oss, indent + 1);
                if (child.kind_ == EKind::Object && !child.children_.empty()) {
                    oss << label << ":\n" << child.PrettyPrint(indent + 2) << "\n";
                } else {
                    oss << label << ": " << child.PrettyPrint(0) << "\n";
                }
            }
            IndentTo(oss, indent);
            oss << "}";
        }
        break;
    }
    return oss.str();
}

std::string TAstNode::KindName(int kind)
{
    switch (static_cast<EKind>(kind)) {
    case EKind::Null:   return "Null";
    case EKind::Value:  return "Value";
    case EKind::Object: return "Object";
    }
    return "?";
}

std::string TAstNode::DiffWith(const TAstNode& other) const
{
    if (*this == other) {
        return "";
    }
    return DiffImpl(other, "");
}

std::string TAstNode::DiffImpl(const TAstNode& other, const std::string& path) const
{
    if (kind_ != other.kind_) {
        return path + ": kind mismatch (" + KindName(static_cast<int>(kind_)) +
               " vs " + KindName(static_cast<int>(other.kind_)) + ")";
    }
    if (content_ != other.content_) {
        return path + ": content mismatch (\"" + content_ + "\" vs \"" +
               other.content_ + "\")";
    }
    if (children_.size() != other.children_.size()) {
        return path + ": children count mismatch (" +
               std::to_string(children_.size()) + " vs " +
               std::to_string(other.children_.size()) + ")";
    }
    auto it_a = children_.begin();
    auto it_b = other.children_.begin();
    size_t i = 0;
    for (; it_a != children_.end(); ++it_a, ++it_b, ++i) {
        if (it_a->first != it_b->first) {
            return path + ": child label[" + std::to_string(i) + "] mismatch (\"" +
                   it_a->first + "\" vs \"" + it_b->first + "\")";
        }
        const std::string sub = it_a->second.DiffImpl(it_b->second,
                                                     path + "/" + it_a->first);
        if (!sub.empty()) {
            return sub;
        }
    }
    return "";
}
